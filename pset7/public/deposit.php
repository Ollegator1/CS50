<?php
    # buy.php
    // configuration
    require("../includes/config.php");
    // if user reached page via GET (as by clicking a link or via redirect)
    if ($_SERVER["REQUEST_METHOD"] == "GET")
    {
        // render form
        render("deposit_form.php", ["title" => "Deposit"]);
    }
    // else if user reached page via POST (as by submitting a form via POST)
    if ($_SERVER["REQUEST_METHOD"] == "POST")
    {
        // TODO
        if (isset($_POST["deposit"]) && is_numeric($_POST["deposit"]) && $_POST["deposit"] > 0)
            $deposit = $_POST["deposit"];
        else
            apologize("Value must be greater than 0.00.");
            
        CS50::query("UPDATE users SET cash = cash + ? WHERE id = ?", $deposit, $_SESSION["id"]);
        
        // We would like to format in US Dollars with a Dollar sign
        setlocale(LC_MONETARY, 'en_US.UTF-8');
        $new = CS50::query("SELECT cash FROM users WHERE id = ?", $_SESSION["id"]);
        render("deposit.php", ["title" => "Deposit", "deposit" => money_format('%.2n', $deposit),
        "balance" => money_format('%.2n', $balance[0]["cash"]), "new" => money_format('%.2n', $new[0]["cash"])]);        
    }