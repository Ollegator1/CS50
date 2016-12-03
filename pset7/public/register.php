<?php
    # register.php
    // configuration
    require("../includes/config.php");
    // if user reached page via GET (as by clicking a link or via redirect)
    if ($_SERVER["REQUEST_METHOD"] == "GET")
    {
        // else render form
        render("register_form.php", ["title" => "Register"]);
    }
    // else if user reached page via POST (as by submitting a form via POST)
    else if ($_SERVER["REQUEST_METHOD"] == "POST")
    {
        // TODO
        if (empty($_POST["username"]))
        {
            apologize("You must provide your username.");
        }
        else if (empty($_POST["password"]))
        {
            apologize("You must provide your password.");
        }
        else if ($_POST["confirmation"] != $_POST["password"])
        {
            apologize("Passwords do not match");
        }
        else
        {
            // If all of the above checks pass, attempt to register the user.
            $result = CS50::query("SELECT * FROM users WHERE username = ?", $_POST["username"]);
            if (!empty($result))
                apologize("The username is already registered");
            else
            {
                $result = CS50::query("INSERT IGNORE INTO users (username, hash, cash) VALUES(?, ?, 10000.0000)",
                                      $_POST["username"], password_hash($_POST["password"], PASSWORD_DEFAULT));
                // Want to log the use in automatically
                $rows = CS50::query("SELECT LAST_INSERT_ID() AS id");
                $id = $rows[0]["id"];
                $_SESSION["id"] = $id;
                redirect("/");
            }
        }
    }
?>