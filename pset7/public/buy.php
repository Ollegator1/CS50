<?php
    # buy.php
    // configuration
    require("../includes/config.php");
    // if user reached page via GET (as by clicking a link or via redirect)
    if ($_SERVER["REQUEST_METHOD"] == "GET")
    {
        // else render form
        render("buy_form.php", ["title" => "Splurge?"]);
    }
    // else if user reached page via POST (as by submitting a form via POST)
    if ($_SERVER["REQUEST_METHOD"] == "POST")
    {
        // handle unsuccessful cases first
        if (empty($_POST["symbol"]) && empty($_POST["shares"]))
            apologize("Complete both fields.");
            
        if (!ctype_alpha($_POST["symbol"]))
            apologize("Symbol must be alphabetical.");
        
        // either you're spending money, or get out
        if ($_POST["shares"] <= 0)
            apologize("Shares must be greater than zero.");
            
        // gather the stock's information, ensuring that symbols are upper cased
        $id = $_SESSION["id"];
        $shares = $_POST["shares"];
        $symbol =  strtoupper(htmlspecialchars($_POST["symbol"]));
        $stock = lookup($symbol);
 
         // check for whole increments os shares
        if (!preg_match("/^\d+$/", $_POST["shares"]))
            apologize("Shares must be in whole increments.");
        // calculate cost of shares
        $cost = $shares * floatval($stock["price"]); 
        // check client's cash
        $cash = CS50::query("SELECT cash FROM users WHERE id = ?", $id);
        // not overspending
        if ($cash[0]["cash"] < $cost)
            apologize("Insufficient funds.");
        # all checks have passed
        
        // add purchased shares to client's portfolio
        CS50::query("INSERT INTO portfolios (user_id, symbol, shares) 
                     VALUES(?, ?, ?) ON DUPLICATE KEY 
                     UPDATE shares = shares + VALUES(shares)", 
                     $id, $symbol, $shares);
        
        // debit the client's balance, because there are no free lunches
        CS50::query("UPDATE users SET cash = cash - '$cost' WHERE id = ?", $id);
        
        // update client's history'
        $transaction = "BUY";
        CS50::query("INSERT INTO history (user_id, transaction, symbol, shares, price, `timestamp`)
                    VALUES(?, ?, ?, ?, ?, NOW())", $id, $transaction, $symbol, $shares, 
                    floatval($stock["price"]));
        redirect("/");
    }
?>