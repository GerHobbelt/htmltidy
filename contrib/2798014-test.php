<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
 <title></title>
  <meta http-equiv="content-type" content="text/html; charset=iso-8859-1" />
</head>
<body style="background: #606060;">
  <div style="width: 600px; margin: auto; background: #004DBF; color: #fff; padding: 10px;">
<?php
 if ($fp = @fopen("../data/database.php", "w")) {
    fwrite($fp, '<?php\n\$databasename=\"' . $databasename . '\";\n?>\n');
     fclose($fp);
     } else die("Error - unable to write to database.php");
 $sqldb = sqlite_open("../data/$databasename.db") or die ("Couldn't open database: " . sqlite_error_string(sqlite_last_error($sqldb)));
?>
</div>
</body>
</html>