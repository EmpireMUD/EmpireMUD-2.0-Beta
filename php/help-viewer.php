<?php
/**
* help-viewer.php
* EmpireMUD 2.0 web-based help files
* by Paul S. Clarke
*
* This program searches your help files from a web browser, which can be
* helpful both for players and for content builders.
*
* Using this file:
*  1. update the configuration, near the top
*  2. you may want to symlink this file into your web directory; it's also
*     very plain looking so you may want to put it into an iframe or style it
*     yourself
*/


// CONFIGURE: path to help index, ending in /
$indexPath = '/path/to/empireMUD/lib/text/help/';


// input
$q = isset($_GET['q']) ? $_GET['q'] : '';
$allowimm = isset($_GET['imm']) && $_GET['imm'];

// data sotrage
global $helpFiles;
$helpFiles = array();

// import help files
$list = file_get_contents($indexPath . 'index');
foreach (explode("\n", $list) as $fname) {
	if (preg_match('/\.hlp$/', $fname)) {
		// echo "importing $indexPath$fname...\n";
		import_help(file_get_contents($indexPath . $fname));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BASIC DISPLAY ///////////////////////////////////////////////////////////
?>

<style>
	div.result {
		font-family: "Lucida Console", Monaco, monospace;
		font-size: 10pt;
		margin: 5px;
		padding: 5px;
		border: 1px solid gray;
	}
</style>

<form action="<?= $_SERVER['SCRIPT_NAME'] ?>" method="GET">
	<input type="text" name="q" placeholder="search" value="<?= $q ?>" size="40" tabindex="1" />
	<input type="submit" value="search" tabindex="2" />
	<input type="checkbox" id="allowimm" name="imm" value="1" tabindex="3" <?= ($allowimm ? 'checked="checked"' : '') ?> /> <label for="allowimm">search immortal helps too</label>
</form>


<?php

 //////////////////////////////////////////////////////////////////////////////
//// SHOW RESULTS ////////////////////////////////////////////////////////////

if ($q) {
	foreach ($helpFiles as $help) {
		if (($allowimm || !$help->isImmortal()) && $help->isKeyword($q)) {
			echo "<div class=\"result\">", $help->showBody(), "</div>\n";
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPER STUFF ////////////////////////////////////////////////////////////


// helper for importing
function import_help($raw) {
	global $helpFiles;
	
	$working = FALSE;
	$body = '';
	foreach (explode("\n", $raw) as $line) {
		if (!$working) {
			$working = TRUE;
			$body = '';
		}
		if ($line[0] == '#') {
			$helpFiles[] = new HelpFile($body, substr($line, 1));
			$working = FALSE;
		}
		else if ($working) {
			$body .= $line . "\n";
		}
		
		// done
		if (!$working && $line[0] == '$') {
			return;
		}
	}
}

class HelpFile {
	/**
	* constructor -- pass a raw help file from keyword line to BEFORE the #
	*
	* @param string $raw The raw help file.
	* @param 
	*/
	public function __construct($raw, $level = '') {
		// process keywords
		$first = strtok($raw, "\n");
		preg_match_all('/"(?:\\\\.|[^\\\\"])*"|\S+/', $first, $matches);
		foreach ($matches[0] as $str) {
			$this->keywords[] = str_replace('"', '', $str);
		}
		
		// store data
		$this->body = $raw;
		$this->level = $level;
	}
	
	/**
	* @return bool TRUE if there is no access restriction, FALSE if there is
	*/
	public function isImmortal() {
		return $this->level != '';
	}
	
	/**
	* @return string The body of the help file, for html.
	*/
	public function showBody() {
		$str = $this->body;
		
		// strip color codes
		$str = preg_replace('/([^&])&[rbgywpalomcjtvun0RBGYWPALOMCJTVU]/', '$1', $str);
		
		$replacements = array(
			"&&" => "&amp;",
			"<" => "&lt;",	">" => "&gt;",
			" " => "&nbsp;",
			"\n" => "<br/>",
		);
		
		$str = str_replace(array_keys($replacements), array_values($replacements), $str);
		
		return $str;
	}
	
	/**
	* @param string $str A keyword/phrase.
	* @return bool TRUE if the keyword matches.
	*/
	public function isKeyword($str) {
		foreach ($this->keywords as $word) {
			if (!strncmp(strtoupper($str), $word, strlen($str))) {
				return TRUE;
			}
		}
		return FALSE;
	}

	// data
	private $keywords;	// array of strings
	private $level;	// level code, if any
	private $body;	// text of help including keyword header
}
