<?php
/**
* map-viewer.php
* EmpireMUD 2.0 map viewer
* by Paul S. Clarke
*
* This program displays your generated graphical maps with a toggle for a
* 100-tile grid, political mode (switches between two maps), and with mouseover
* coordinates using javascript.
*
* Using this file:
*  1. update the configuration options, near the top
*  2. you may want to symlink this file into your web directory
*  3. you can adjust the size by adding ?size=1234 -- Must be in pixels; this
*     sets the width of the map, and the height scales proportionately.
*/

// CONFIGURE THESE (you can include paths; these are used in img tags)
$normalMap = 'map.png';
$politicalMap = 'map-political.png';
$mapWidth = 1800;
$mapHeight = 1000;
$minRadiusToName = 10;	// cities below this size won't show name/owner

// Where to load city data:
$pathToData = '/path/to/empireMUD/data/';	// your path, ending in 'data/'

$cityList = array();
if (file_exists($pathToData . 'map-cities.txt')) {
	$cityInput = file_get_contents($pathToData . 'map-cities.txt');
	foreach (explode("\n", $cityInput) as $line) {
		if (preg_match('/^(\d+)\s+(\d+)\s+(\d+)\s+"([^"]+)"\s+"([^"]+)"$/', $line, $match)) {
			$cityList[] = array(
				'x' => $match[1],
				'y' => $match[2],
				'radius' => $match[3],
				'city' => $match[4],
				'empire' => $match[5],
			);
		}
	}
}

// the rest of this generates the page
$defaultWidth = $mapWidth;
$scale = (isset($_GET['size']) ? $_GET['size'] : $defaultWidth) / $defaultWidth;
$width = $defaultWidth * $scale;
$height = $mapHeight * $scale;
?>
<!doctype html>
<html lang="en">

<head>
<meta charset="UTF-8">
<title>EmpireMUD Map Viewer</title>
<style type="text/css">

body {
	font-family: monospace;
}

#map {
	width: <?= $width ?>px;
	height: <?= $height ?>px;
	padding: 0px;
	margin: 0px auto;
	position: relative;
	overflow: hidden;
	clear: both;
}

.wld {
	position: absolute;
	top: 0px;
	left: 0px;
	width: <?= $width ?>px;
	height: <?= $height ?>px;
}

.ver {
	position: absolute;
	top: 0px;
	border-left: 1px solid red;
	border-right: 1px solid red;
	width: <?= 100 * $scale ?>px;
	height: <?= $height ?>px;
}

.hor {
	position: absolute;
	left: 0px;
	border-top: 1px solid red;
	border-bottom: 1px solid red;
	width: <?= $width ?>px;
	height: <?= 100 * $scale ?>px;
}

.equator {
	position: absolute;
	left: 0;x;
	border-top: 2px dashed blue;
	width: <?= $width ?>px;
}

.h1 {
	top: 100px;
}
.h2 {
	top: 300px;
}
.h3 {
	top: 500px;
}

#coords {
	text-align: center;
	background-color: #F0E68C;
	border-right: 1px solid #8B814C;
	border-bottom: 1px solid #8B814C;
	float: left;
	font-family: monospace;
	display: none;
	position: absolute;
	top: 0;
	left: 0;
	z-index: 500;
}

#prefs {
	width: <?= $width ?>px;
	margin: 0px auto 3px auto;
}
#prefs div {
	float: left;
	margin-right: 30px;
}

.city {
	position: absolute;
	left: 0px;
	top: 0px;
	border: 1px outset gray;
	display: none;
	cursor: default;
}

</style>
</head>
<body>

<div id="prefs">
	<div title="Press 'p' to toggle the political map."><input type="checkbox" id="pol" name="pol" /> <label for="pol">Political Map</label></div>
	<div title="Press 'g' to toggle the grid."><input type="checkbox" id="grid" name="grid" /> <label for="grid">Grid</label></div>
	<div title="Press 'c' to toggle cities."><input type="checkbox" id="cities" name="cities" /> <label for="cities">Cities</label></div>
	<div style="clear: both;"></div>
</div>
<div id="coords"></div>
<div id="map">
<img src="" class="wld" />
<?php
// city overlays
foreach ($cityList as $city) {
	?>
	<div class="city" style="top: <?= $height - (($city['y'] + $city['radius']) * $scale) - 1.5 ?>px; left: <?= (($city['x'] - $city['radius']) * $scale) - 0.75 ?>px; width: <?= $city['radius'] * 2 * $scale ?>px; height: <?= $city['radius'] * 2 * $scale ?>px; border-radius: <?= $city['radius'] * $scale ?>px;" title="<?= ($city['radius'] >= $minRadiusToName) ? ($city['city'] . ' (' . $city['empire'] . ')') : 'Outpost' ?>">&nbsp;</div>
	<?php
}

// show grid: verticals
for ($iter = 1; $iter < $width / 100 / $scale; ++$iter) {
	if ($iter % 2) {
		?><div class="ver" style="left: <?= $iter * $scale * 100 ?>px;"></div><?php
	}
}
// show grid: horizonticals
for ($iter = 1; $iter < $height / 100 / $scale; ++$iter) {
	if ($iter % 2) {
		?><div class="hor" style="top: <?= $iter * $scale * 100 ?>px;"></div><?php
	}
}
?>
<div class="equator" style="top: <?= $height/2 ?>px;"</div>
</div>


<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js"></script>
<script type="text/javascript">
function checkgrid() {
	if ($("#grid").prop("checked")) {
		$(".ver, .hor").show();
	}
	else {
		$(".ver, .hor").hide();
	}
}
function checkmap() {
	if ($("#pol").prop("checked")) {
		$(".wld").attr("src", "<?= $politicalMap ?>");
	}
	else {
		$(".wld").attr("src", "<?= $normalMap ?>");
	}
}
function checkcities() {
	if ($("#cities").prop("checked")) {
		$(".city").show();
	}
	else {
		$(".city").hide();
	}
}

$(document).ready(function() {
	checkmap();
	checkgrid();
	checkcities();

	$("#pol").click(function() {
		checkmap();
	});
	
	$("#grid").click(function() {
		checkgrid();
	});
	
	$("#cities").click(function() {
		checkcities();
	});
	
	$(document).keypress(function(e) {
		var c = String.fromCharCode(e.which);
		if (c == 'p' || c == 'P') {
			$("#pol").click();
		}
		else if (c == 'g' || c == 'G') {
			$("#grid").click();
		}
		else if (c == 'c' || c == 'C') {
			$("#cities").click();
		}
	});

	$("#map").bind("mouseover mouseout mousemove", function(e) {
		if (e.type == "mouseout") {
			$("#coords").hide();
		}
		else {
			//var parentOffset = $(this).parent().offset(); 
			var parentOffset = $(this).offset(); 
			//or $(this).offset(); if you really just want the current element's offset
			var relX = parseInt((e.pageX - parentOffset.left) / <?= $scale ?>);
			var relY = parseInt((<?= $height ?> - e.pageY + parentOffset.top) / <?= $scale ?>);

			var coordW = 100;
			var coordH = 20;

			var leftl = e.pageX + 10;
			var topl = e.pageY + 10;

			if (leftl + coordW > $(window).width() + $(window).scrollLeft()) {
				leftl += $(window).width() + $(window).scrollLeft() - (leftl + coordW);
			}
			if (topl + coordH > $(window).height() + $(window).scrollTop()) {
				topl = e.pageY - 20;
			}

			$("#coords").html("("+relX+",&nbsp;"+relY+")");
			$("#coords").css('top', topl).css('left', leftl);
			$("#coords").show();
		}
	});
});
</script>
</body>
</html>
