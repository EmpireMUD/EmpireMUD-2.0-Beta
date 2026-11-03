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
			'date' => date('F j, Y', $date),
			'isoDate' => date('Y-m-d', $date),	// machine-readable date for month/year seeking
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
	<input type="button" id="prevYear" value="<<< Year" title="Back one year" />
	<input type="button" id="prevMonth" value="<< Month" title="Back one month" />
	<input type="button" id="prevDay" value="< Day" title="Back one day" />
	<input type="button" id="start" value="Start" />
	<input type="button" id="stop" value="Stop" />
	<input type="button" id="nextDay" value="Day >" title="Ahead one day" />
	<input type="button" id="nextMonth" value="Month >>" title="Ahead one month" />
	<input type="button" id="nextYear" value="Year >>>" title="Ahead one year" />
</div>

<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js"></script>
<script type="text/javascript">
var data = <?= json_encode(array_values($imgList)) ?>;
var pos = -1, timer;

$(document).ready(function() {
	startShow();

	// month/year buttons jump to the map closest to that calendar date
	$("#prevYear").click(function() {
		seekByMonths(-12);
	});
	$("#prevMonth").click(function() {
		seekByMonths(-1);
	});
	$("#nextMonth").click(function() {
		seekByMonths(1);
	});
	$("#nextYear").click(function() {
		seekByMonths(12);
	});

	// day buttons simply step to the adjacent map
	$("#prevDay").click(function() {
		stopShow();
		setShow(pos-1);
	});
	$("#nextDay").click(function() {
		stopShow();
		setShow(pos+1);
	});

	// start/stop buttons start and stop the animation
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

/**
* Splits a "YYYY-MM-DD" string into its numeric parts.
*
* @param string isoDateString A date like "2015-01-31".
* @return object { year, month, day } where month is 1-12.
*/
function parseIsoDate(isoDateString) {
	var dateParts = isoDateString.split('-');
	return {
		year: Number(dateParts[0]),
		month: Number(dateParts[1]),
		day: Number(dateParts[2])
	};
}

/**
* Converts a calendar date to milliseconds, using UTC so that daylight-saving
* changes don't skew the distance between two dates.
*
* @param object calendarDate A { year, month, day } object (month is 1-12).
* @return number Milliseconds since the Unix epoch.
*/
function calendarDateToUtcMilliseconds(calendarDate) {
	return Date.UTC(calendarDate.year, calendarDate.month - 1, calendarDate.day);
}

/**
* Moves a calendar date forward or back by whole months. The day is clamped to
* the end of the target month, so Jan 31 + 1 month is Feb 28/29, not March.
*
* @param object calendarDate A { year, month, day } object (month is 1-12).
* @param int monthOffset How many months to move (negative goes back).
* @return object A new { year, month, day } object.
*/
function addMonthsClamped(calendarDate, monthOffset) {
	// count months from year 0 so the offset can cross year boundaries
	var totalMonths = calendarDate.year * 12 + (calendarDate.month - 1) + monthOffset;
	var targetYear = Math.floor(totalMonths / 12);
	var targetMonth = totalMonths - targetYear * 12 + 1;

	// day 0 of the following month is the last day of the target month
	var daysInTargetMonth = new Date(targetYear, targetMonth, 0).getDate();

	return {
		year: targetYear,
		month: targetMonth,
		day: Math.min(calendarDate.day, daysInTargetMonth)
	};
}

/**
* Stops the animation and jumps to the map whose date is closest to the
* current map's date shifted by monthOffset. Thumbnails aren't generated every
* day, so an exact match may not exist.
*
* @param int monthOffset Months to move: -12/12 for a year, -1/1 for a month.
*/
function seekByMonths(monthOffset) {
	stopShow();
	if (!data.length) {
		return;
	}

	// before the first frame is shown, seek from the oldest map
	var currentIndex = pos;
	if (currentIndex < 0 || currentIndex >= data.length) {
		currentIndex = 0;
	}

	var currentDate = parseIsoDate(data[currentIndex].isoDate);
	var targetMilliseconds = calendarDateToUtcMilliseconds(addMonthsClamped(currentDate, monthOffset));

	// find the map closest to the target date, starting from the current one
	var closestIndex = currentIndex;
	var closestDistance = Math.abs(calendarDateToUtcMilliseconds(currentDate) - targetMilliseconds);

	for (var candidateIndex = 0; candidateIndex < data.length; candidateIndex++) {
		var candidateDistance = Math.abs(calendarDateToUtcMilliseconds(parseIsoDate(data[candidateIndex].isoDate)) - targetMilliseconds);

		// on a tie when seeking forward, prefer the later map so the button
		// never appears stuck
		var isCloser = (candidateDistance < closestDistance);
		var isLaterTieGoingForward = (candidateDistance === closestDistance && monthOffset > 0 && candidateIndex > closestIndex);

		if (isCloser || isLaterTieGoingForward) {
			closestDistance = candidateDistance;
			closestIndex = candidateIndex;
		}
	}

	setShow(closestIndex);
}

</script>
</body>
</html>
