<?php
/**
* map-viewer.php
* EmpireMUD 2.0 map viewer
* by Paul S. Clarke
*
* This program displays your generated graphical maps with a toggle for a
* 100-tile grid, political mode (switches between two maps), and with mouseover
* coordinates using javascript. Drag to pan; use the mouse wheel (or trackpad
* pinch) to zoom toward the cursor. Double-click the map to reset the view.
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
	cursor: default;
	touch-action: none;
	user-select: none;
	-webkit-user-select: none;
}

#map-stage {
	position: absolute;
	top: 0;
	left: 0;
	width: <?= $width ?>px;
	height: <?= $height ?>px;
	transform-origin: 0 0;
	will-change: transform;
}

.wld {
	position: absolute;
	top: 0px;
	left: 0px;
	width: <?= $width ?>px;
	height: <?= $height ?>px;
	pointer-events: none;
}

.ver {
	position: absolute;
	top: 0px;
	border-left: 1px solid red;
	border-right: 1px solid red;
	width: <?= 100 * $scale ?>px;
	height: <?= $height ?>px;
	z-index: 5;
}

.hor {
	position: absolute;
	left: 0px;
	border-top: 1px solid red;
	border-bottom: 1px solid red;
	width: <?= $width ?>px;
	height: <?= 100 * $scale ?>px;
	z-index: 5;
}

.equator {
	position: absolute;
	left: 0;
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
	cursor: inherit;
	z-index: 10;
}

.underline {
	text-decoration: underline;
}

.pixelated {
	image-rendering:optimizeSpeed;
	image-rendering:-moz-crisp-edges;
	image-rendering:-o-crisp-edges;
	image-rendering:-webkit-optimize-contrast;
	image-rendering:optimize-contrast;
	image-rendering:crisp-edges;
	image-rendering:pixelated;
	-ms-interpolation-mode:nearest-neighbor;
}

</style>
</head>
<body>

<div id="prefs">
	<div title="Press 'p' to toggle the political map."><input type="checkbox" id="pol" name="pol" /> <label for="pol"><span class="underline">P</span>olitical Map</label></div>
	<div title="Press 'g' to toggle the grid."><input type="checkbox" id="grid" name="grid" /> <label for="grid"><span class="underline">G</span>rid</label></div>
	<div title="Press 'c' to toggle cities."><input type="checkbox" id="cities" name="cities" /> <label for="cities"><span class="underline">C</span>ities</label></div>
	<div title="Press 'x' to toggle pixelation while zoomed."><input type="checkbox" id="pixelate" name="pixelate" checked /> <label for="pixelate">Pi<span class="underline">x</span>elate When Zoomed</label></div>
	<div title="Drag to pan, scroll/pinch to zoom, double-click or press 'r' to reset.">Drag / scroll to pan &amp; zoom (<span class="underline">R</span>eset)</div>
	<div style="clear: both;"></div>
</div>
<div id="coords"></div>
<div id="map" title="Drag to pan, scroll to zoom, double-click to reset">
<div id="map-stage">
<img src="" class="wld pixelated" />
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
<div class="equator" style="top: <?= $height/2 ?>px;"></div>
</div>
</div>


<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js"></script>
<script type="text/javascript">
/** @type {number} Display width of the map viewport in pixels. */
var mapWidth = <?= $width ?>;
/** @type {number} Display height of the map viewport in pixels. */
var mapHeight = <?= $height ?>;
/** @type {number} Pixels-per-tile scale from the PHP ?size= parameter. */
var tileScale = <?= $scale ?>;
/** @type {number} Minimum allowed zoom level (1 = fit viewport). */
var minZoom = 1;
/** @type {number} Maximum allowed zoom level. */
var maxZoom = 16;
/** @type {number} Current zoom multiplier applied to #map-stage. */
var zoom = 1;
/** @type {number} Current horizontal pan offset of #map-stage in viewport pixels. */
var panX = 0;
/** @type {number} Current vertical pan offset of #map-stage in viewport pixels. */
var panY = 0;
/** @type {boolean} True while the primary mouse button is held for panning. */
var isDragging = false;
/** @type {boolean} True if the current drag moved enough to count as a pan. */
var didDrag = false;
/** @type {number} pageX where the current drag started. */
var dragStartX = 0;
/** @type {number} pageY where the current drag started. */
var dragStartY = 0;
/** @type {number} panX value captured at the start of the current drag. */
var panStartX = 0;
/** @type {number} panY value captured at the start of the current drag. */
var panStartY = 0;

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
function checkpixelate() {
	if ($("#pixelate").prop("checked")) {
		$(".wld").addClass("pixelated");
	}
	else {
		$(".wld").removeClass("pixelated");
	}
}

/**
 * Keeps panX/panY so the map cannot be dragged fully out of the viewport.
 * When zoomed out to fit, centers the stage instead.
 */
function clampPan() {
	var viewW = mapWidth;
	var viewH = mapHeight;
	var contentW = mapWidth * zoom;
	var contentH = mapHeight * zoom;

	if (contentW <= viewW) {
		panX = (viewW - contentW) / 2;
	}
	else {
		panX = Math.min(0, Math.max(viewW - contentW, panX));
	}

	if (contentH <= viewH) {
		panY = (viewH - contentH) / 2;
	}
	else {
		panY = Math.min(0, Math.max(viewH - contentH, panY));
	}
}

/**
 * Clamps the current pan and applies translate/scale to #map-stage.
 */
function applyViewTransform() {
	clampPan();
	$("#map-stage").css("transform", "translate(" + panX + "px, " + panY + "px) scale(" + zoom + ")");
}

/**
 * Restores zoom and pan to the default 1x, top-left view.
 */
function resetView() {
	zoom = 1;
	panX = 0;
	panY = 0;
	applyViewTransform();
}

/**
 * Zooms the map toward a document point so that point stays under the cursor.
 *
 * @param {number} pageX Document X coordinate to keep anchored (usually event.pageX).
 * @param {number} pageY Document Y coordinate to keep anchored (usually event.pageY).
 * @param {number} nextZoom Desired zoom level before min/max clamping.
 */
function zoomAt(pageX, pageY, nextZoom) {
	nextZoom = Math.min(maxZoom, Math.max(minZoom, nextZoom));
	if (nextZoom === zoom) {
		return;
	}

	var offset = $("#map").offset();
	var viewX = pageX - offset.left;
	var viewY = pageY - offset.top;
	var contentX = (viewX - panX) / zoom;
	var contentY = (viewY - panY) / zoom;

	zoom = nextZoom;
	panX = viewX - contentX * zoom;
	panY = viewY - contentY * zoom;
	applyViewTransform();
}

/**
 * Converts a mouse event into game-map tile coordinates, accounting for pan/zoom.
 *
 * @param {JQuery.Event|MouseEvent} e Mouse event with pageX/pageY.
 * @return {{x: number, y: number}} Tile coordinates under the cursor.
 */
function mapCoordsFromEvent(e) {
	var offset = $("#map").offset();
	var viewX = e.pageX - offset.left;
	var viewY = e.pageY - offset.top;
	var contentX = (viewX - panX) / zoom;
	var contentY = (viewY - panY) / zoom;
	return {
		x: parseInt(contentX / tileScale, 10),
		y: parseInt((mapHeight - contentY) / tileScale, 10)
	};
}

/**
 * Shows the floating coordinate tooltip near the cursor for the given mouse event.
 *
 * @param {JQuery.Event|MouseEvent} e Mouse event used for position and tile lookup.
 */
function updateCoordsDisplay(e) {
	var coords = mapCoordsFromEvent(e);
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

	$("#coords").html("(" + coords.x + ",&nbsp;" + coords.y + ")");
	$("#coords").css("top", topl).css("left", leftl);
	$("#coords").show();
}

$(document).ready(function() {
	checkmap();
	checkgrid();
	checkcities();
	checkpixelate();
	applyViewTransform();

	$("#pol").click(function() {
		checkmap();
	});

	$("#grid").click(function() {
		checkgrid();
	});

	$("#cities").click(function() {
		checkcities();
	});

	$("#pixelate").click(function() {
		checkpixelate();
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
		else if (c == 'x' || c == 'X') {
			$("#pixelate").click();
		}
		else if (c == 'r' || c == 'R') {
			resetView();
		}
	});

	$("#map").on("wheel", function(e) {
		e.preventDefault();
		var oe = e.originalEvent;
		var delta = oe.deltaY;
		// normalize trackpad/mouse wheel deltas into a smooth zoom factor
		var factor = Math.exp(-delta * 0.0015);
		zoomAt(oe.pageX, oe.pageY, zoom * factor);
		updateCoordsDisplay(e);
	});

	$("#map").on("mousedown", function(e) {
		if (e.which !== 1) {
			return;
		}
		isDragging = true;
		didDrag = false;
		dragStartX = e.pageX;
		dragStartY = e.pageY;
		panStartX = panX;
		panStartY = panY;
		e.preventDefault();
	});

	$(document).on("mousemove", function(e) {
		if (isDragging) {
			var dx = e.pageX - dragStartX;
			var dy = e.pageY - dragStartY;
			if (Math.abs(dx) > 2 || Math.abs(dy) > 2) {
				didDrag = true;
			}
			panX = panStartX + dx;
			panY = panStartY + dy;
			applyViewTransform();
		}

		var offset = $("#map").offset();
		var overMap = (
			e.pageX >= offset.left &&
			e.pageX <= offset.left + mapWidth &&
			e.pageY >= offset.top &&
			e.pageY <= offset.top + mapHeight
		);
		if (overMap) {
			updateCoordsDisplay(e);
		}
		else if (!isDragging) {
			$("#coords").hide();
		}
	});

	$(document).on("mouseup", function() {
		if (!isDragging) {
			return;
		}
		isDragging = false;
	});

	$("#map").on("mouseleave", function() {
		if (!isDragging) {
			$("#coords").hide();
		}
	});

	$("#map").on("dblclick", function(e) {
		e.preventDefault();
		resetView();
		updateCoordsDisplay(e);
	});

	// ignore click side-effects after a drag (e.g. accidental text selection)
	$("#map").on("click", function(e) {
		if (didDrag) {
			e.preventDefault();
			e.stopImmediatePropagation();
			didDrag = false;
		}
	});
});
</script>
</body>
</html>
