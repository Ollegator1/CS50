//
// server.c
//
// Ollegator1
// Computer Science 50
// Problem Set 6
//

// feature test macro requirements
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _XOPEN_SOURCE_EXTENDED

// limits on an HTTP request's size, based on Apache's
// http://httpd.apache.org/docs/2.2/mod/core.html
#define LimitRequestFields 50
#define LimitRequestFieldSize 4094
#define LimitRequestLine 8190

// number of bytes for buffers
#define BYTES 512

// header files
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// types
typedef char BYTE;

// prototypes
bool connected(void);
void error(unsigned short code);
void freedir(struct dirent** namelist, int n);
void handler(int signal);
char* htmlspecialchars(const char* s);
char* indexes(const char* path);
void interpret(const char* path, const char* query);
void list(const char* path);
bool load(FILE* file, BYTE** content, size_t* length);
const char* lookup(const char* path);
bool parse(const char* line, char* path, char* query);
const char* reason(unsigned short code);
void redirect(const char* uri);
bool request(char** message, size_t* length);
void respond(int code, const char* headers, const char* body, size_t length);
void start(short port, const char* path);
void stop(void);
void transfer(const char* path, const char* type);
char* urldecode(const char* s);

// server's root
char* root = NULL;

// file descriptor for sockets
int cfd = -1, sfd = -1;

// flag indicating whether control-c has been heard
bool signaled = false;

int main(int argc, char* argv[])
{
    // a global variable defined in errno.h that's "set by system 
    // calls and some library functions [to a nonzero value]
    // in the event of an error to indicate what went wrong"
    errno = 0;

    // default to port 8080
    int port = 8080;

    // usage
    const char* usage = "Usage: server [-p port] /path/to/root";

    // parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "hp:")) != -1)
    {
        switch (opt)
        {
            // -h
            case 'h':
                printf("%s\n", usage);
                return 0;

            // -p port
            case 'p':
                port = atoi(optarg);
                break;
        }
    }

    // ensure port is a non-negative short and path to server's root is specified
    if (port < 0 || port > SHRT_MAX || argv[optind] == NULL || strlen(argv[optind]) == 0)
    {
        // announce usage
        printf("%s\n", usage);

        // return 2 just like bash's builtins
        return 2;
    }

    // start server
    start(port, argv[optind]);

    // listen for SIGINT (aka control-c)
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    // a message and its length
    char* message = NULL;
    size_t length = 0;

    // path requested
    char* path = NULL;

    // accept connections one at a time
    while (true)
    {
        // free last path, if any
        if (path != NULL)
        {
            free(path);
            path = NULL;
        }

        // free last message, if any
        if (message != NULL)
        {
            free(message);
            message = NULL;
        }
        length = 0;

        // close last client's socket, if any
        if (cfd != -1)
        {
            close(cfd);
            cfd = -1;
        }

        // check for control-c
        if (signaled)
        {
            stop();
        }

        // check whether client has connected
        if (connected())
        {
            // check for request
            if (request(&message, &length))
            {
                // extract message's request-line
                // http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html
                const char* haystack = message;
                const char* needle = strstr(haystack, "\r\n");
                if (needle == NULL)
                {
                    error(500);
                    continue;
                }
                char line[needle - haystack + 2 + 1];
                strncpy(line, haystack, needle - haystack + 2);
                line[needle - haystack + 2] = '\0';

                // log request-line
                printf("%s", line);

                // parse request-line
                char abs_path[LimitRequestLine + 1];
                char query[LimitRequestLine + 1];
                if (parse(line, abs_path, query))
                {
                    // URL-decode absolute-path
                    char* p = urldecode(abs_path);
                    if (p == NULL)
                    {
                        error(500);
                        continue;
                    }

                    // resolve absolute-path to local path
                    path = malloc(strlen(root) + strlen(p) + 1);
                    if (path == NULL)
                    {
                        error(500);
                        continue;
                    }
                    strcpy(path, root);
                    strcat(path, p);
                    free(p);

                    // ensure path exists
                    if (access(path, F_OK) == -1)
                    {
                        error(404);
                        continue;
                    }

                    // if path to directory
                    struct stat sb;
                    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode))
                    {
                        // redirect from absolute-path to absolute-path/
                        if (abs_path[strlen(abs_path) - 1] != '/')
                        {
                            char uri[strlen(abs_path) + 1 + 1];
                            strcpy(uri, abs_path);
                            strcat(uri, "/");
                            redirect(uri);
                            continue;
                        }

                        // use path/index.php or path/index.html, if present, instead of directory's path
                        char* index = indexes(path);
                        if (index != NULL)
                        {
                            free(path);
                            path = index;
                        }

                        // list contents of directory
                        else
                        {
                            list(path);
                            continue;
                        }
                    }

                    // look up MIME type for file at path
                    const char* type = lookup(path);
                    if (type == NULL)
                    {
                        error(501);
                        continue;
                    }

                    // interpret PHP script at path
                    if (strcasecmp("text/x-php", type) == 0)
                    {
                        interpret(path, query);
                    }

                    // transfer file at path
                    else
                    {
                        transfer(path, type);
                    }
                }
            }
        }
    }
}

/**
 * Checks (without blocking) whether a client has connected to server. 
 * Returns true iff so.
 */
bool connected(void)
{
    struct sockaddr_in cli_addr;
    memset(&cli_addr, 0, sizeof(cli_addr));
    socklen_t cli_len = sizeof(cli_addr);
    cfd = accept(sfd, (struct sockaddr*) &cli_addr, &cli_len);
    if (cfd == -1)
    {
        return false;
    }
    return true;
}

/**
 * Responds to client with specified status code.
 */
void error(unsigned short code)
{
    // determine code's reason-phrase
    const char* phrase = reason(code);
    if (phrase == NULL)
    {
        return;
    }

    // template for response's content
    char* template = "<html><head><title>%i %s</title></head><body><h1>%i %s</h1></body></html>";

    // render template
    char body[(strlen(template) - 2 - ((int) log10(code) + 1) - 2 + strlen(phrase)) * 2 + 1];
    int length = sprintf(body, template, code, phrase, code, phrase);
    if (length < 0)
    {
        body[0] = '\0';
        length = 0;
    }

    // respond with error
    char* headers = "Content-Type: text/html\r\n";
    respond(code, headers, body, length);
}

/**
 * Frees memory allocated by scandir.
 */
void freedir(struct dirent** namelist, int n)
{
    if (namelist != NULL)
    {
        for (int i = 0; i < n; i++)
        {
            free(namelist[i]);
        }
        free(namelist);
    }
}
 
/**
 * Handles signals.
 */
void handler(int signal)
{
    // control-c
    if (signal == SIGINT)
    {
        signaled = true;
    }
}

/**
 * Escapes string for HTML. Returns dynamically allocated memory for escaped
 * string that must be deallocated by caller.
 */
char* htmlspecialchars(const char* s)
{
    // ensure s is not NULL
    if (s == NULL)
    {
        return NULL;
    }

    // allocate enough space for an unescaped copy of s
    char* t = malloc(strlen(s) + 1);
    if (t == NULL)
    {
        return NULL;
    }
    t[0] = '\0';

    // iterate over characters in s, escaping as needed
    for (int i = 0, old = strlen(s), new = old; i < old; i++)
    {
        // escape &
        if (s[i] == '&')
        {
            const char* entity = "&amp;";
            new += strlen(entity);
            t = realloc(t, new);
            if (t == NULL)
            {
                return NULL;
            }
            strcat(t, entity);
        }

        // escape "
        else if (s[i] == '"')
        {
            const char* entity = "&quot;";
            new += strlen(entity);
            t = realloc(t, new);
            if (t == NULL)
            {
                return NULL;
            }
            strcat(t, entity);
        }

        // escape '
        else if (s[i] == '\'')
        {
            const char* entity = "&#039;";
            new += strlen(entity);
            t = realloc(t, new);
            if (t == NULL)
            {
                return NULL;
            }
            strcat(t, entity);
        }

        // escape <
        else if (s[i] == '<')
        {
            const char* entity = "&lt;";
            new += strlen(entity);
            t = realloc(t, new);
            if (t == NULL)
            {
                return NULL;
            }
            strcat(t, entity);
        }

        // escape >
        else if (s[i] == '>')
        {
            const char* entity = "&gt;";
            new += strlen(entity);
            t = realloc(t, new);
            if (t == NULL)
            {
                return NULL;
            }
            strcat(t, entity);
        }

        // don't escape
        else
        {
            strncat(t, s + i, 1);
        }
    }

    // escaped string
    return t;
}

/**
 * Checks, in order, whether index.php or index.html exists inside of path.
 * Returns path to first match if so, else NULL.
 */
char* indexes(const char* path)
{
    // If path contains a . (ASCII 46) return NULL
    if (strchr(path, 46) != NULL) {
        return NULL;
    }
    
    // Check for index.html
    char* index_html_path = calloc(strlen(path) + 11, 1);
    index_html_path = strcat(index_html_path, path);
    if (path[strlen(path) - 1] != '/') {
        index_html_path = strcat(index_html_path, "/");
    }
    index_html_path = strcat(index_html_path, "index.html");
    if (access(index_html_path, R_OK) == 0) {
        return index_html_path;
    }
    else {
        free(index_html_path);
    }
    
    // Check for index.php
    char* index_php_path = calloc(strlen(path) + 10, 1);
    index_php_path = strcat(index_php_path, path);
    if (path[strlen(path) - 1] != '/') {
        index_php_path = strcat(index_php_path, "/");
    }
    index_php_path = strcat(index_php_path, "index.php");
    if (access(index_php_path, R_OK) == 0) {
        return index_php_path;
    }
    else {
        return NULL;
    }
}

/**
 * Interprets PHP file at path using query string.
 */
void interpret(const char* path, const char* query)
{
    // ensure path is readable
    if (access(path, R_OK) == -1)
    {
        error(403);
        return;
    }

    // open pipe to PHP interpreter
    char* format = "QUERY_STRING=\"%s\" REDIRECT_STATUS=200 SCRIPT_FILENAME=\"%s\" php-cgi";
    char command[strlen(format) + (strlen(path) - 2) + (strlen(query) - 2) + 1];
    if (sprintf(command, format, query, path) < 0)
    {
        error(500);
        return;
    }
    FILE* file = popen(command, "r");
    if (file == NULL)
    {
        error(500);
        return;
    }

    // load interpreter's content
    char* content;
    size_t length;
    if (load(file, &content, &length) == false)
    {
        error(500);
        return;
    }

    // close pipe
    pclose(file);

    // subtract php-cgi's headers from content's length to get body's length
    char* haystack = content;
    char* needle = strstr(haystack, "\r\n\r\n");
    if (needle == NULL)
    {
        free(content);
        error(500);
        return;
    }

    // extract headers
    char headers[needle + 2 - haystack + 1];
    strncpy(headers, content, needle + 2 - haystack);
    headers[needle + 2 - haystack] = '\0';

    // respond with interpreter's content
    respond(200, headers, needle + 4, length - (needle - haystack + 4));

    // free interpreter's content
    free(content);
}

/**
 * Responds to client with directory listing of path.
 */
void list(const char* path)
{
    // ensure path is readable and executable
    if (access(path, R_OK | X_OK) == -1)
    {
        error(403);
        return;
    }

    // open directory
    DIR* dir = opendir(path);
    if (dir == NULL)
    {
        return;
    }

    // buffer for list items
    char* list = malloc(1);
    list[0] = '\0';

    // iterate over directory entries
    struct dirent** namelist = NULL;
    int n = scandir(path, &namelist, NULL, alphasort);
    for (int i = 0; i < n; i++)
    {
        // omit . from list
        if (strcmp(namelist[i]->d_name, ".") == 0)
        {
            continue;
        }

        // escape entry's name
        char* name = htmlspecialchars(namelist[i]->d_name);
        if (name == NULL)
        {
            free(list);
            freedir(namelist, n);
            error(500);
            return;
        }

        // append list item to buffer
        char* template = "<li><a href=\"%s\">%s</a></li>";
        list = realloc(list, strlen(list) + strlen(template) - 2 + strlen(name) - 2 + strlen(name) + 1);
        if (list == NULL)
        {
            free(name);
            freedir(namelist, n);
            error(500);
            return;
        }
        if (sprintf(list + strlen(list), template, name, name) < 0)
        {
            free(name);
            freedir(namelist, n);
            free(list);
            error(500);
            return;
        }

        // free escaped name
        free(name);
    }

    // free memory allocated by scandir
    freedir(namelist, n);

    // prepare response
    const char* relative = path + strlen(root);
    char* template = "<html><head><title>%s</title></head><body><h1>%s</h1><ul>%s</ul></body></html>";
    char body[strlen(template) - 2 + strlen(relative) - 2 + strlen(relative) - 2 + strlen(list) + 1];
    int length = sprintf(body, template, relative, relative, list);
    if (length < 0)
    {
        free(list);
        closedir(dir);
        error(500);
        return;
    }

    // free buffer
    free(list);

    // close directory
    closedir(dir);

    // respond with list
    char* headers = "Content-Type: text/html\r\n";
    respond(200, headers, body, length);
}

/**
 * Loads a file into memory dynamically allocated on heap.
 * Stores address thereof in *content and length thereof in *length.
 */
bool load(FILE* file, BYTE** content, size_t* length)
{
    
    // Create temporary variable to buffer the reads
    // BYTE read_byte_buffer = 0;
    int buffer = 0;
    
    // Create pointer to store file contents
    BYTE* file_contents = calloc(1, 1);
    
    // Create index counter
    int index = 0;
    
    // Read file to file_contents
    while ((buffer = fgetc(file)) != EOF) {
        
        // Get the next character
        // buffer = fgetc(file);
        
        // Add buffer to contents
        *(file_contents + index) = buffer;
        // file_contents[index] =  buffer;
        
        // Increment the index
        index++;
        
        // Extend file_contents
        file_contents = realloc(file_contents, index + 1);
        
    }
    
    if (file_contents != NULL) {
        
        // Write EOF
        // *(file_contents + index) = buffer;

        // Assign file_contents pointer to *content
        *content = file_contents;
        // memcpy(*content, file_contents, index + 1);

        // Assign index + 1 to length
        *length = index;

        return true;

    }
    
    else {
        return false;
    }
    
}

/**
 * Returns MIME type for supported extensions, else NULL.
 */
const char* lookup(const char* path)
{
    // Return NULL if no path
    if (strlen(path) == 0) {
        return NULL;
    }
    
    // use strrchr() to find last . (ASCII 46) in path
    char* dot_ptr = strrchr(path, 46);

    // create mime
    char* mime = malloc(17);
    // char mime[16] = {0};

    // check for file extensions and strcopy MIME types
    if (dot_ptr == NULL) {
        return NULL;
    }
    else if (strcasecmp(dot_ptr, ".css") == 0)
    {
        strcpy(mime, "text/css");
    }
    else if (strcasecmp(dot_ptr, ".html") == 0)
    {
        strcpy(mime, "text/html");
    }
    else if (strcasecmp(dot_ptr, ".gif") == 0)
    {
        strcpy(mime, "image/gif");
    }
    else if (strcasecmp(dot_ptr, ".ico") == 0)
    {
        strcpy(mime, "image/x-icon");
    }
    else if (strcasecmp(dot_ptr, ".jpg") == 0)
    {
       strcpy(mime, "image/jpeg");
    }
    else if (strcasecmp(dot_ptr, ".js") == 0)
    {
       strcpy(mime, "text/javascript");
    }
    else if (strcasecmp(dot_ptr, ".php") == 0)
    {
       strcpy(mime, "text/x-php");
    }
    else if (strcasecmp(dot_ptr, ".png") == 0)
    {
       strcpy(mime, "image/png");
    }
    else {
        
        // If none of the above
        free(mime);
        return NULL;
        
    }
    
    // Return mime
    // free(mime);
    return mime;
    
}

/**
 * Parses a request-line, storing its absolute-path at abs_path 
 * and its query string at query, both of which are assumed
 * to be at least of length LimitRequestLine + 1.
 */
bool parse(const char* line, char* path, char* query)
{
    
    // REF: "method request-target HTTP-version\r\n"
    
    // Look for "GET "; trigger 405 and return false if not

    // Create temporary method string
    char method[] = "0000";
    
    // Copy in first four characters of line
    strncpy(method, line, 4);
    
    // Check for "GET "
    if (strcmp(method, "GET ") != 0) {
        error(405);
        return false;
    }

    // Find spaces (ASCII 32) in line to clarify target start and end locations
    char* first_space_ptr = strchr(line, 32);
    char* second_space_ptr = strrchr(line, 32);
    
    // 400 any requests with no spaces
    if (first_space_ptr == NULL || second_space_ptr == NULL) {
        error(400);
        return false;
    }
    
    // If not HTTP/1.1, trigger 505 and return false
    if (strcasecmp(second_space_ptr, " HTTP/1.1\r\n") != 0) {
        error(505);
        return false;
    }
    
    // Define full target
    char* target = malloc(strlen(line));
    // char target[LimitRequestLine + 1];
    strncpy(target, first_space_ptr + 1, second_space_ptr - first_space_ptr);
    
    // Add null terminator to target
    target[second_space_ptr - first_space_ptr - 1] = '\0';
    
    // Look for spaces in target; trigger 400 and return false if found
    if (strchr(target, 32) != NULL) {
        error(400);
        free(target);
        return false;
    }

    // Look for / at start of target; trigger 501 and return false if not
    if (target[0] != '/') {
        error(501);
        free(target);
        return false;
    }
    
    // Look for errant quotation marks; trigger 400 and return false if found
    char* quote_ptr = strstr(target, "\"");
    if (quote_ptr != NULL) {
        error(400);
        free(target);
        return false;
    }
    
    // Look for ? in target, split target into path and query if necessary
    char* question_ptr = strstr(target, "?");
    
    if (question_ptr == NULL) {
        // path = target;
        strcpy(path, target);
        free(target);
        return true;
    }
    else {
        
        // Define path_loc with pointer arithmatic
        char* path_loc = malloc(strlen(line));
        strncpy(path_loc, target, question_ptr - target);

        // Add null terminator to path
        path_loc[question_ptr - target] = '\0';
        
        // Assign path and target
        strcpy(path, path_loc);
        question_ptr = question_ptr + 1;
        strcpy(query, question_ptr);
        free(target);
        free(path_loc);
        return true;
    }
}

/**
 * Returns status code's reason phrase.
 *
 * http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6
 * https://tools.ietf.org/html/rfc2324
 */
const char* reason(unsigned short code)
{
    switch (code)
    {
        case 200: return "OK";
        case 301: return "Moved Permanently";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 414: return "Request-URI Too Long";
        case 418: return "I'm a teapot";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 505: return "HTTP Version Not Supported";
        default: return NULL;
    }
}

/**
 * Redirects client to uri.
 */
void redirect(const char* uri)
{
    char* template = "Location: %s\r\n";
    char headers[strlen(template) - 2 + strlen(uri) + 1];
    if (sprintf(headers, template, uri) < 0)
    {
        error(500);
        return;
    }
    respond(301, headers, NULL, 0);
}

/**
 * Reads (without blocking) an HTTP request's headers into memory dynamically allocated on heap.
 * Stores address thereof in *message and length thereof in *length.
 */
bool request(char** message, size_t* length)
{
    // ensure socket is open
    if (cfd == -1)
    {
        return false;
    }

    // initialize message and its length
    *message = NULL;
    *length = 0;

    // read message 
    while (*length < LimitRequestLine + LimitRequestFields * LimitRequestFieldSize + 4)
    {
        // read from socket
        BYTE buffer[BYTES];
        ssize_t bytes = read(cfd, buffer, BYTES);
        if (bytes < 0)
        {
            if (*message != NULL)
            {
                free(*message);
                *message = NULL;
            }
            *length = 0;
            break;
        }

        // append bytes to message 
        *message = realloc(*message, *length + bytes + 1);
        if (*message == NULL)
        {
            *length = 0;
            break;
        }
        memcpy(*message + *length, buffer, bytes);
        *length += bytes;

        // null-terminate message thus far
        *(*message + *length) = '\0';

        // search for CRLF CRLF
        int offset = (*length - bytes < 3) ? *length - bytes : 3;
        char* haystack = *message + *length - bytes - offset;
        char* needle = strstr(haystack, "\r\n\r\n");
        if (needle != NULL)
        {
            // trim to one CRLF and null-terminate
            *length = needle - *message + 2;
            *message = realloc(*message, *length + 1);
            if (*message == NULL)
            {
                break;
            }
            *(*message + *length) = '\0';

            // ensure request-line is no longer than LimitRequestLine
            haystack = *message;
            needle = strstr(haystack, "\r\n");
            if (needle == NULL || (needle - haystack + 2) > LimitRequestLine)
            {
                break;
            }

            // count fields in message
            int fields = 0;
            haystack = needle + 2;
            while (*haystack != '\0')
            {
                // look for CRLF
                needle = strstr(haystack, "\r\n");
                if (needle == NULL)
                {
                    break;
                }

                // ensure field is no longer than LimitRequestFieldSize
                if (needle - haystack + 2 > LimitRequestFieldSize)
                {
                    break;
                }

                // look beyond CRLF
                haystack = needle + 2;
            }

            // if we didn't get to end of message, we must have erred
            if (*haystack != '\0')
            {
                break;
            }

            // ensure message has no more than LimitRequestFields
            if (fields > LimitRequestFields)
            {
                break;
            }

            // valid
            return true;
        }
    }

    // invalid
    if (*message != NULL)
    {
        free(*message);
    }
    *message = NULL;
    *length = 0;
    return false;
}

/**
 * Responds to a client with status code, headers, and body of specified length.
 */
void respond(int code, const char* headers, const char* body, size_t length)
{
    // determine Status-Line's phrase
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1
    const char* phrase = reason(code);
    if (phrase == NULL)
    {
        return;
    }

    // respond with Status-Line
    if (dprintf(cfd, "HTTP/1.1 %i %s\r\n", code, phrase) < 0)
    {
        return;
    }

    // respond with headers
    if (dprintf(cfd, "%s", headers) < 0)
    {
        return;
    }

    // respond with CRLF
    if (dprintf(cfd, "\r\n") < 0)
    {
        return;
    }

    // respond with body
    if (write(cfd, body, length) == -1)
    {
        return;
    }

    // log response line
    if (code == 200)
    {
        // green
        printf("\033[32m");
    }
    else
    {
        // red
        printf("\033[33m");
    }
    printf("HTTP/1.1 %i %s", code, phrase);
    printf("\033[39m\n");
}

/**
 * Starts server on specified port rooted at path.
 */
void start(short port, const char* path)
{
    // path to server's root
    root = realpath(path, NULL);
    if (root == NULL)
    {
        stop();
    }

    // ensure root is executable
    if (access(root, X_OK) == -1)
    {
        stop();
    }

    // announce root
    printf("\033[33m");
    printf("Using %s for server's root", root);
    printf("\033[39m\n");

    // create a socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        stop();
    }

    // allow reuse of address (to avoid "Address already in use")
    int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // assign name to socket
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
    {
        printf("\033[33m");
        printf("Port %i already in use", port);
        printf("\033[39m\n");
        stop();
    }

    // listen for connections
    if (listen(sfd, SOMAXCONN) == -1)
    {
        stop();
    }

    // announce port in use
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if (getsockname(sfd, (struct sockaddr*) &addr, &addrlen) == -1)
    {
        stop();
    }
    printf("\033[33m");
    printf("Listening on port %i", ntohs(addr.sin_port));
    printf("\033[39m\n");
}

/**
 * Stop server, deallocating any resources.
 */
void stop(void)
{
    // preserve errno across this function's library calls
    int errsv = errno;

    // announce stop
    printf("\033[33m");
    printf("Stopping server\n");
    printf("\033[39m");

    // free root, which was allocated by realpath
    if (root != NULL)
    {
        free(root);
    }

    // close server socket
    if (sfd != -1)
    {
        close(sfd);
    }

    // stop server
    exit(errsv);
}

/**
 * Transfers file at path with specified type to client.
 */
void transfer(const char* path, const char* type)
{
    // ensure path is readable
    if (access(path, R_OK) == -1)
    {
        error(403);
        return;
    }

    // open file
    FILE* file = fopen(path, "r");
    if (file == NULL)
    {
        error(500);
        return;
    }

    // load file's content
    BYTE* content;
    size_t length;
    if (load(file, &content, &length) == false)
    {
        error(500);
        return;
    }

    // close file
    fclose(file);

    // prepare response
    char* template = "Content-Type: %s\r\n";
    char headers[strlen(template) - 2 + strlen(type) + 1];
    if (sprintf(headers, template, type) < 0)
    {
        error(500);
        return;
    }

    // respond with file's content
    respond(200, headers, content, length);

    // free file's content
    free(content);
}

/**
 * URL-decodes string, returning dynamically allocated memory for decoded string
 * that must be deallocated by caller.
 */
char* urldecode(const char* s)
{
    // check whether s is NULL
    if (s == NULL)
    {
        return NULL;
    }

    // allocate enough (zeroed) memory for an undecoded copy of s
    char* t = calloc(strlen(s) + 1, 1);
    if (t == NULL)
    {
        return NULL;
    }
    
    // iterate over characters in s, decoding percent-encoded octets, per
    // https://www.ietf.org/rfc/rfc3986.txt
    for (int i = 0, j = 0, n = strlen(s); i < n; i++, j++)
    {
        if (s[i] == '%' && i < n - 2)
        {
            char octet[3];
            octet[0] = s[i + 1];
            octet[1] = s[i + 2];
            octet[2] = '\0';
            t[j] = (char) strtol(octet, NULL, 16);
            i += 2;
        }
        else if (s[i] == '+')
        {
            t[j] = ' ';
        }
        else
        {
            t[j] = s[i];
        }
    }

    // escaped string
    return t;
}