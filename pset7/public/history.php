<?php
    # history.php
    // configuration
    require("../includes/config.php");
    $transactions = [];
    // retrieve current user's transactions (stocks)
    $rows = CS50::query("SELECT * FROM history WHERE user_id = ?", $_SESSION["id"]);
    
    // no records found
    if (empty($rows))
        render("history.php", ["title" => "History", "transactions" => $transactions]);
        
    // convert sql string representation to unix time stamp
    $udate = (strtotime($rows[0]["timestamp"]));
    $datetime = date("d-m-y H:i:s", $udate);
    
    // We would like to format in US Dollars with a Dollar sign
    setlocale(LC_MONETARY, 'en_US.UTF-8');
    foreach ($rows as $row)
    {
        $stock = lookup($row["symbol"]);
        if ($stock !== false)
        {
            $transactions[] = [
                "transaction" => $row["transaction"],
                "timestamp" => $datetime,
                "symbol" => $row["symbol"],
                "shares" => $row["shares"],
                "price" => money_format('%.2n', $row["price"])
            ];
        }
    }
    // render user's portfolio
    render("history.php", ["title" => "History", "transactions" => $transactions]);
?>