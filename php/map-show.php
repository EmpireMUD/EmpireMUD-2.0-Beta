<?php
/**
* map-show.php
* EmpireMUD 2.0 thumbnail viewer
* by Paul S. Clarke
*
* NOTE: You MUST update anything that says /path/to/ below.
*
* This php program reads the list of maps created by map-thumb-generator.php
* and plays them as an animation using javascript. You can load this page
* inside an iframe to put it in any other web page.
*
* Using this file:
*  1. you must have thumbnails created by map-thumb-generator.php in a web dir
*  2. update the $basePath and $webDir variables near the top of this file
*  3. symlink this php file into your live web directory and open it in a
*     web browser
*/

// CONFIGURE THIS:
// the absolute path to your thumbnails
$basePath = '/path/to/html/docroot/old-maps';
// the path the web page uses to view that dir
$webDir = '/old-maps';


// odn't edit below here
$raw = glob("{$basePath}/*.png");

$imgList = array();
foreach ($raw as $file) {
	if (preg_match('/\/map-(\d+)-(\d+)-(\d+)\.png$/', $file, $match)) {
		$date = strtotime(sprintf("%04d-%02d-%02d", $match[3], $match[1], $match[2]));
		$imgList[$date] = array(
			'file' => $webDir . '/' . basename($file),
			'date' => date('F Y', $date),
		);
	}
}

// sort by date
ksort($imgList);

?>
<!doctype html>
<html lang="en">

<head>
<meta charset="UTF-8">
<title>EmpireMUD Map Viewer</title>
<link rel="stylesheet" href="zoom/css/anythingzoomer.css" />
<style type="text/css">

#date,
#ctrls {
	font-family: sans-serif;
	width: 600px;
	text-align: center;
}

#show {
	width: 600px;
	/* height: 600px; */
}

</style>
</head>
<body>

<div id="date"></div>
<img src="" id="show" />
<div id="ctrls">
	<input type="button" id="prev" value="<<" />
	<input type="button" id="start" value="Start" />
	<input type="button" id="stop" value="Stop" />
	<input type="button" id="next" value=">>" />
</div>

<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js"></script>
<script type="text/javascript">
var data = <?= json_encode(array_values($imgList)) ?>;
var pos = -1, timer;

$(document).ready(function() {
	startShow();
	
	$("#prev").click(function() {
		stopShow();
		setShow(pos-1);
	});
	$("#next").click(function() {
		stopShow();
		setShow(pos+1);
	});
	$("#start").click(function() {
		stopShow();
		startShow();
	});
	$("#stop").click(function() {
		stopShow();
	});
});

function setShow(to) {
	pos = to;

	// bounds-wrapping
	if (pos > (data.length-1)) {
		pos = 0;
	}
	if (pos < 0) {
		pos = data.length-1;
	}
	
	var nextpos = pos+1;
	if (nextpos > (data.length-1)) {
		nextpos = 0;
	}
	
	$("#date").html(data[pos].date);
	$("#show").attr("src", data[pos].file);

	// preload
	$('<img/>')[0].src = data[nextpos].file;
}

function startShow() {
	timer = setInterval(function() {
		setShow(pos+1);
	}, 1000);
}

function stopShow() {
	clearInterval(timer);
}

</script>
</body>
</html>
