<?php
/**
* map-viewer.php
* EmpireMUD 2.0 map viewer
* by Paul S. Clarke
*
* This program displays your generated graphical maps with a toggle for a
* 100-tile grid, political mode (switches between two maps), and with mouseover
* coordinates using javascript. Drag to pan; use the mouse wheel (or trackpad /
* touch pinch) to zoom toward the pointer. Double-click / double-tap the map
* (or press 'r') to reset the view. The map scales down to fit the browser
* viewport so page scrollbars are not needed.
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
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>EmpireMUD Map Viewer</title>
<style type="text/css">

html, body {
	margin: 0;
	padding: 0;
	overflow: hidden;
	height: 100%;
}

body {
	font-family: monospace;
}

#prefs {
	display: flex;
	flex-wrap: wrap;
	align-items: baseline;
	width: 100%;
	max-width: 100%;
	box-sizing: border-box;
	margin: 0 auto;
	padding: 6px 8px 3px 8px;
}

#prefs div {
	margin-right: 20px;
	margin-bottom: 4px;
}

#map {
	width: <?= $width ?>px;
	height: <?= $height ?>px;
	padding: 0;
	margin: 0 auto;
	position: relative;
	overflow: hidden;
	clear: both;
	cursor: default;
	touch-action: none;
	user-select: none;
	-webkit-user-select: none;
	background: #111;
}

#map-stage {
	position: absolute;
	top: 0;
	left: 0;
	transform-origin: 0 0;
}

.wld {
	position: absolute;
	top: 0;
	left: 0;
	width: <?= $width ?>px;
	height: <?= $height ?>px;
	pointer-events: none;
	display: block;
}

#map-overlays {
	position: absolute;
	top: 0;
	left: 0;
	width: <?= $width ?>px;
	height: <?= $height ?>px;
	transform-origin: 0 0;
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
	pointer-events: none;
}

.city {
	position: absolute;
	left: 0px;
	top: 0px;
	border: 1px outset gray;
	display: none;
	cursor: inherit;
	z-index: 10;
	pointer-events: auto;
}

.underline {
	text-decoration: underline;
}

/*
 * Pixelation must be applied via layout width/height scaling on the <img>,
 * not via transform: scale() on an ancestor — Chrome ignores image-rendering
 * for transform-scaled bitmaps (Firefox still honors it).
 */
.pixelated {
	image-rendering: -moz-crisp-edges;
	image-rendering: crisp-edges;
	image-rendering: pixelated;
	-ms-interpolation-mode: nearest-neighbor;
}

</style>
</head>
<body>

<div id="prefs">
	<div title="Press 'p' to toggle the political map."><input type="checkbox" id="pol" name="pol" /> <label for="pol"><span class="underline">P</span>olitical Map</label></div>
	<div title="Press 'g' to toggle the grid."><input type="checkbox" id="grid" name="grid" /> <label for="grid"><span class="underline">G</span>rid</label></div>
	<div title="Press 'c' to toggle cities."><input type="checkbox" id="cities" name="cities" /> <label for="cities"><span class="underline">C</span>ities</label></div>
	<div title="Press 'x' to toggle pixelation while zoomed."><input type="checkbox" id="pixelate" name="pixelate" checked /> <label for="pixelate">Pi<span class="underline">x</span>elate When Zoomed</label></div>
	<div title="Drag to pan, scroll/pinch to zoom, double-click/double-tap or press 'r' to reset.">Drag/pinch to pan &amp; zoom (<span class="underline">R</span>eset)</div>
</div>
<div id="coords"></div>
<div id="map" title="Drag to pan, scroll/pinch to zoom, double-click or double-tap to reset">
<div id="map-stage">
<img src="" class="wld pixelated" alt="EmpireMUD map" />
<div id="map-overlays">
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
</div>


<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/2.0.0/jquery.min.js"></script>
<script type="text/javascript">
/** @type {number} Logical (unfitted) map content width in pixels. */
var contentWidth = <?= $width ?>;
/** @type {number} Logical (unfitted) map content height in pixels. */
var contentHeight = <?= $height ?>;
/** @type {number} Current map viewport width in pixels. */
var viewWidth = contentWidth;
/** @type {number} Current map viewport height in pixels. */
var viewHeight = contentHeight;
/** @type {number} Pixels-per-tile scale from the PHP ?size= parameter. */
var tileScale = <?= $scale ?>;
/** @type {number} Zoom level that fits the full map in the viewport. */
var minZoom = 1;
/** @type {number} Maximum allowed zoom level. */
var maxZoom = 16;
/** @type {number} Current zoom multiplier relative to logical content size. */
var zoom = 1;
/** @type {number} Current horizontal pan offset of #map-stage in viewport pixels. */
var panX = 0;
/** @type {number} Current vertical pan offset of #map-stage in viewport pixels. */
var panY = 0;
/** @type {boolean} True while a one-pointer pan is in progress. */
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
/** @type {number} Ignore emulated mouse events until this timestamp (ms). */
var ignoreMouseUntil = 0;
/** @type {number} Timestamp of the last completed single-finger tap. */
var lastTapTime = 0;
/** @type {number} Distance between two fingers when a pinch began. */
var pinchStartDistance = 0;
/** @type {number} Zoom level captured when a pinch began. */
var pinchStartZoom = 1;
/** @type {boolean} True while a two-finger pinch zoom is active. */
var isPinching = false;

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
 * Sizes #map to the largest aspect-correct rectangle that fits in the window
 * and updates minZoom so the full map is visible without page scrollbars.
 */
function updateViewportSize() {
	var prefsHeight = $("#prefs").outerHeight(true) || 0;
	var availW = Math.max(100, window.innerWidth);
	var availH = Math.max(100, window.innerHeight - prefsHeight);
	var aspect = contentWidth / contentHeight;
	var nextViewW;
	var nextViewH;

	if ((availW / availH) > aspect) {
		nextViewH = Math.min(availH, contentHeight);
		nextViewW = nextViewH * aspect;
	}
	else {
		nextViewW = Math.min(availW, contentWidth);
		nextViewH = nextViewW / aspect;
	}

	viewWidth = Math.floor(nextViewW);
	viewHeight = Math.floor(nextViewH);
	minZoom = viewWidth / contentWidth;
	maxZoom = Math.max(16, minZoom * 16);

	$("#map").css({
		width: viewWidth + "px",
		height: viewHeight + "px"
	});

	if (zoom < minZoom) {
		zoom = minZoom;
	}
}

/**
 * Keeps panX/panY so the map cannot be dragged fully out of the viewport.
 * When the scaled content is smaller than the viewport, centers the stage.
 */
function clampPan() {
	var contentW = contentWidth * zoom;
	var contentH = contentHeight * zoom;

	if (contentW <= viewWidth) {
		panX = (viewWidth - contentW) / 2;
	}
	else {
		panX = Math.min(0, Math.max(viewWidth - contentW, panX));
	}

	if (contentH <= viewHeight) {
		panY = (viewHeight - contentH) / 2;
	}
	else {
		panY = Math.min(0, Math.max(viewHeight - contentH, panY));
	}
}

/**
 * Clamps pan and applies translate / layout-size zoom.
 * The image is resized via width/height so Chrome honors image-rendering.
 * Overlays use transform: scale() so city/grid markup need not be rewritten.
 */
function applyViewTransform() {
	clampPan();
	$("#map-stage").css("transform", "translate(" + panX + "px, " + panY + "px)");
	$(".wld").css({
		width: (contentWidth * zoom) + "px",
		height: (contentHeight * zoom) + "px"
	});
	$("#map-overlays").css("transform", "scale(" + zoom + ")");
}

/**
 * Restores zoom/pan to the fitted, fully-visible view.
 */
function resetView() {
	zoom = minZoom;
	panX = 0;
	panY = 0;
	applyViewTransform();
}

/**
 * Zooms the map toward a document point so that point stays under the pointer.
 *
 * @param {number} pageX Document X coordinate to keep anchored.
 * @param {number} pageY Document Y coordinate to keep anchored.
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
 * Converts a pointer position into game-map tile coordinates.
 *
 * @param {number} pageX Document X coordinate.
 * @param {number} pageY Document Y coordinate.
 * @return {{x: number, y: number}} Tile coordinates under the pointer.
 */
function mapCoordsFromPagePoint(pageX, pageY) {
	var offset = $("#map").offset();
	var viewX = pageX - offset.left;
	var viewY = pageY - offset.top;
	var contentX = (viewX - panX) / zoom;
	var contentY = (viewY - panY) / zoom;
	return {
		x: parseInt(contentX / tileScale, 10),
		y: parseInt((contentHeight - contentY) / tileScale, 10)
	};
}

/**
 * Shows the floating coordinate tooltip near a document point.
 *
 * @param {number} pageX Document X coordinate of the pointer.
 * @param {number} pageY Document Y coordinate of the pointer.
 */
function updateCoordsDisplayAt(pageX, pageY) {
	var coords = mapCoordsFromPagePoint(pageX, pageY);
	var coordW = 100;
	var coordH = 20;
	var leftl = pageX + 10;
	var topl = pageY + 10;

	if (leftl + coordW > $(window).width() + $(window).scrollLeft()) {
		leftl += $(window).width() + $(window).scrollLeft() - (leftl + coordW);
	}
	if (topl + coordH > $(window).height() + $(window).scrollTop()) {
		topl = pageY - 20;
	}

	$("#coords").html("(" + coords.x + ",&nbsp;" + coords.y + ")");
	$("#coords").css("top", topl).css("left", leftl);
	$("#coords").show();
}

/**
 * @param {number} pageX Document X coordinate.
 * @param {number} pageY Document Y coordinate.
 * @return {boolean} True if the point lies inside the map viewport.
 */
function isOverMap(pageX, pageY) {
	var offset = $("#map").offset();
	return (
		pageX >= offset.left &&
		pageX <= offset.left + viewWidth &&
		pageY >= offset.top &&
		pageY <= offset.top + viewHeight
	);
}

/**
 * Starts a one-pointer pan from a document coordinate.
 *
 * @param {number} pageX Document X coordinate.
 * @param {number} pageY Document Y coordinate.
 */
function beginDrag(pageX, pageY) {
	isDragging = true;
	didDrag = false;
	dragStartX = pageX;
	dragStartY = pageY;
	panStartX = panX;
	panStartY = panY;
}

/**
 * Continues an active one-pointer pan.
 *
 * @param {number} pageX Document X coordinate.
 * @param {number} pageY Document Y coordinate.
 */
function moveDrag(pageX, pageY) {
	if (!isDragging) {
		return;
	}
	var dx = pageX - dragStartX;
	var dy = pageY - dragStartY;
	if (Math.abs(dx) > 2 || Math.abs(dy) > 2) {
		didDrag = true;
	}
	panX = panStartX + dx;
	panY = panStartY + dy;
	applyViewTransform();
}

/**
 * Ends an active one-pointer pan.
 */
function endDrag() {
	isDragging = false;
}

/**
 * @param {TouchList|Array} touches Active touch points.
 * @return {number} Distance between the first two touches.
 */
function touchDistance(touches) {
	var dx = touches[0].pageX - touches[1].pageX;
	var dy = touches[0].pageY - touches[1].pageY;
	return Math.sqrt(dx * dx + dy * dy);
}

/**
 * @param {TouchList|Array} touches Active touch points.
 * @return {{pageX: number, pageY: number}} Midpoint between the first two touches.
 */
function touchMidpoint(touches) {
	return {
		pageX: (touches[0].pageX + touches[1].pageX) / 2,
		pageY: (touches[0].pageY + touches[1].pageY) / 2
	};
}

/**
 * Refits the viewport and keeps the current view valid after a window resize.
 */
function handleResize() {
	var previousMinZoom = minZoom;
	var wasFitted = Math.abs(zoom - previousMinZoom) < 0.0001;
	updateViewportSize();
	if (wasFitted || zoom < minZoom) {
		zoom = minZoom;
	}
	applyViewTransform();
}

$(document).ready(function() {
	checkmap();
	checkgrid();
	checkcities();
	checkpixelate();
	updateViewportSize();
	resetView();

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

	$(window).on("resize orientationchange", function() {
		handleResize();
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
		var factor = Math.exp(-oe.deltaY * 0.0015);
		zoomAt(oe.pageX, oe.pageY, zoom * factor);
		updateCoordsDisplayAt(oe.pageX, oe.pageY);
	});

	$("#map").on("mousedown", function(e) {
		if (Date.now() < ignoreMouseUntil || e.which !== 1) {
			return;
		}
		beginDrag(e.pageX, e.pageY);
		e.preventDefault();
	});

	$(document).on("mousemove", function(e) {
		if (Date.now() < ignoreMouseUntil) {
			return;
		}
		moveDrag(e.pageX, e.pageY);
		if (isOverMap(e.pageX, e.pageY)) {
			updateCoordsDisplayAt(e.pageX, e.pageY);
		}
		else if (!isDragging) {
			$("#coords").hide();
		}
	});

	$(document).on("mouseup", function() {
		if (Date.now() < ignoreMouseUntil) {
			return;
		}
		endDrag();
	});

	$("#map").on("mouseleave", function() {
		if (!isDragging) {
			$("#coords").hide();
		}
	});

	$("#map").on("dblclick", function(e) {
		if (Date.now() < ignoreMouseUntil) {
			return;
		}
		e.preventDefault();
		resetView();
		updateCoordsDisplayAt(e.pageX, e.pageY);
	});

	$("#map").on("click", function(e) {
		if (didDrag) {
			e.preventDefault();
			e.stopImmediatePropagation();
			didDrag = false;
		}
	});

	// --- touch / mobile ---
	var mapEl = document.getElementById("map");

	mapEl.addEventListener("touchstart", function(e) {
		ignoreMouseUntil = Date.now() + 700;
		if (e.touches.length === 1) {
			isPinching = false;
			beginDrag(e.touches[0].pageX, e.touches[0].pageY);
		}
		else if (e.touches.length >= 2) {
			endDrag();
			isPinching = true;
			pinchStartDistance = touchDistance(e.touches);
			pinchStartZoom = zoom;
		}
		e.preventDefault();
	}, { passive: false });

	mapEl.addEventListener("touchmove", function(e) {
		ignoreMouseUntil = Date.now() + 700;
		if (isPinching && e.touches.length >= 2) {
			var distance = touchDistance(e.touches);
			if (pinchStartDistance > 0) {
				var mid = touchMidpoint(e.touches);
				zoomAt(mid.pageX, mid.pageY, pinchStartZoom * (distance / pinchStartDistance));
				updateCoordsDisplayAt(mid.pageX, mid.pageY);
			}
		}
		else if (isDragging && e.touches.length === 1) {
			moveDrag(e.touches[0].pageX, e.touches[0].pageY);
			updateCoordsDisplayAt(e.touches[0].pageX, e.touches[0].pageY);
		}
		e.preventDefault();
	}, { passive: false });

	mapEl.addEventListener("touchend", function(e) {
		ignoreMouseUntil = Date.now() + 700;

		if (e.touches.length === 0) {
			var wasPinching = isPinching;
			var dragged = didDrag;
			isPinching = false;
			endDrag();

			// double-tap to reset (ignore if this was a pan or pinch)
			if (!wasPinching && !dragged && e.changedTouches.length === 1) {
				var now = Date.now();
				if (now - lastTapTime < 350) {
					resetView();
					lastTapTime = 0;
				}
				else {
					lastTapTime = now;
					updateCoordsDisplayAt(e.changedTouches[0].pageX, e.changedTouches[0].pageY);
				}
			}
		}
		else if (e.touches.length === 1) {
			// dropped from pinch to one finger — resume pan from remaining touch
			isPinching = false;
			beginDrag(e.touches[0].pageX, e.touches[0].pageY);
		}

		e.preventDefault();
	}, { passive: false });

	mapEl.addEventListener("touchcancel", function() {
		isPinching = false;
		endDrag();
	});
});
</script>
</body>
</html>
