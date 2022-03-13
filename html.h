PROGMEM const char* headerTemplate[] = 
{ 
"<HTML>",
"<HEAD>",
"<TITLE>Settings</TITLE>",
"<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate' />",
"<meta http-equiv='Pragma' content='no-cache' />",
"<meta http-equiv='Expires' content='0' />",
"<script type='text/javascript'>function codeAddress() {history.replaceState(null, '','/');}window.onload = codeAddress;</script>",
"</HEAD>",
"<BODY>",
"<H1>Settings</H1>",
"<form action='/'>",
"<H2>Network</H2>",
"<label for='ip'>IP address: </label><input type='text' id='ip' name='ip' value='$ip'><br><br>",
"<label for='nm'>Netmask: </label><input type='text' id='nm' name='nm' value='$nm'><br><br>",
"<label for='gw'>Gateway: </label><input type='text' id='gw' name='gw' value='$gw'><br><br>",
"<H2>Serial ports</H2>"
};

PROGMEM const char* footerTemplate[] = 
{
"<br><br><input type='submit' value='Update' formaction='/'>",
"<br><br><input type='submit' value='Update & Reboot' formaction='/boot'>",
"</form>",
"</BODY>",
"</HTML>"
};

PROGMEM const char* portTemplate[] = 
{ 
"<div class='serialPort'>",
"<label>port_$</label><br/>",
"<label for='pt_$'>IP port: </label><input type='text' id='pt_$' name='pt_$' value='$pt'><br>",
"<label for='bd_$'>Baud rate:</label>",
"<select name='bd_$' id='bd_$' class='select-baud'>",
"<option value='115200' $sel_115200>115200</option>",
"<option value='57600' $sel_57600>57600</option>",
"<option value='38400' $sel_38400>38400</option>",
"<option value='19200' $sel_19200>19200</option>",
"<option value='9600' $sel_9600>9600</option>",
"<option value='4800' $sel_4800>4800</option>",
"<option value='2400' $sel_2400>2400</option>",
"<option value='1200' $sel_1200>1200</option>",
"</select>",
"<br/>",
"<label for='fl_$'>Flow ctrl:</label>",
"<select name='fl_$' id='fl_$' class='select-flow'>",
"<option value='n' $sel_n>None</option>",
"<option value='r' $sel_r>RTS/CTS</option>",
"<option value='x' $sel_x>XON/XOFF</option>",
"</select>",
"</div>",
"<br/>"
};
