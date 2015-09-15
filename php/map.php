<?php
/**
* map.php
* EmpireMUD 2.0 map jpeg generator
* by Paul S. Clarke
*
* NOTE: You MUST update anything that says /path/to/ below.
*
* This program generates the graphical versions of the game's ascii map, using
* data files output by the mud itself. You can either generate maps on-demand,
* which is slow, or automate this process using a crontab or any automation
* system.
*
* Using this file:
*  1. update $basePath with the path to your mud
*  2. you may want to symlink this file into your web directory
*  3. save maps by running as "php map.php > map.png"
*  4. if you want automatic map generation, consider a crontab entry like this:
*     0,15,30,45 * * * * php /path/to/empiremud/php/map.php > /path/to/public_html/map-new.png && mv /path/to/public_html/map-new.png /path/to/public_html/map.png
*     0,15,30,45 * * * * php /path/to/empiremud/php/map.php political > /path/to/public_html/map-pol-new.png && mv /path/to/public_html/map-pol-new.png /path/to/public_html/map-political.png
*
* You can get three different maps with this tool:
*   Normal map:  php map.php
*   Political map:  php map.php political
*   Generated map:  php map.php generator
*
* See constants.c mapout_color_names and mapout_color_tokens when adding
* colors to this system.
*/

// this utility may need a lot of RAM, briefly, depending on the size of your map
ini_set('memory_limit', '512M');

// CHANGE THIS: the path to your mud
$basePath = "/path/to/empireMUD";


// argument parsing
$type = isset($_GET['type']) ? $_GET['type'] : (count($argv) > 1 ? $argv[1] : NULL);
switch ($type) {
	case 'generated': {
		$file = "{$basePath}/lib/world/wld/map.txt";
		break;
	}
	case 'political': {
		$file = "{$basePath}/data/map-political.txt";
		break;
	}
	default: {
		$file = "{$basePath}/data/map.txt";
		break;
	}
}

$input = file_get_contents($file);

$lines = explode("\n", $input);

$sizeline = array_shift($lines);
if (!preg_match("/(\d+)x(\d+)/", $sizeline, $match)) {
	echo "error reading map size";
	exit;
}
$width = $match[1];
$height = $match[2];

// set up image
$im = imagecreatetruecolor($width, $height);

$data = array(
	'*' => imagecolorallocate($im, 238, 44, 44),	// start location
	'?' => imagecolorallocate($im, 238, 229, 222),	// unclaimed/special use
	
	// banners/bright colors
	'0' => imagecolorallocate($im, 255, 255, 255),	// white banner, rice, cotton
	'1' => imagecolorallocate($im, 238, 0, 0),	// red banner
	'2' => imagecolorallocate($im, 0, 205, 0),	// green banner, peas
	'3' => imagecolorallocate($im, 205, 205, 0),	// yellow banner, corn
	'4' => imagecolorallocate($im, 28, 134, 238),	// blue banner
	'5' => imagecolorallocate($im, 255, 0, 255),	// magenta banner
	'6' => imagecolorallocate($im, 0, 238, 238),	// cyan banner
	
	// terrain colors
	'a' => imagecolorallocate($im, 139, 26, 26),	// cherries
	'b' => imagecolorallocate($im, 144, 238, 144),	// plains, grove
	'c' => imagecolorallocate($im, 127, 255, 0),	// apples
	'd' => imagecolorallocate($im, 102, 205, 170),	// jungle
	'e' => imagecolorallocate($im, 42, 175, 110),	// swamp
	'f' => imagecolorallocate($im, 0, 100, 0),	// forest
	'g' => imagecolorallocate($im, 108, 140, 0),	// olives, hops
	'h' => imagecolorallocate($im, 153, 255, 255),	// tundra
	'i' => imagecolorallocate($im, 0, 191, 255),	// river
	'j' => imagecolorallocate($im, 30, 144, 255),	// oasis
	'k' => imagecolorallocate($im, 79, 148, 205),	// ocean
	'l' => imagecolorallocate($im, 238, 233, 191),	// wheat, barley
	'm' => imagecolorallocate($im, 255, 236, 139),	// desert
	'n' => imagecolorallocate($im, 255, 165, 79),	// peaches
	'o' => imagecolorallocate($im, 238, 154, 0),	// oranges, gourds
	'p' => imagecolorallocate($im, 142, 142, 56),	// trench
	'q' => imagecolorallocate($im, 139, 117, 0),	// mountain
	'r' => imagecolorallocate($im, 193, 193, 193),	// road
	's' => imagecolorallocate($im, 81, 81, 81),	// building
	't' => imagecolorallocate($im, 0, 0, 102),	// Dark Blue
	'u' => imagecolorallocate($im, 0, 76, 153),	// Dark Azure Blue
	'v' => imagecolorallocate($im, 153, 0, 76),	// Dark Magenta
	'w' => imagecolorallocate($im, 0, 153, 153),	// Dark Cyan
	'x' => imagecolorallocate($im, 102, 255, 102),	// Lime Green
	'y' => imagecolorallocate($im, 0, 153, 0),	// Dark Lime Green
	'z' => imagecolorallocate($im, 204, 102, 0),	// Dark Orange
	'A' => imagecolorallocate($im, 255, 153, 204),	// Pink
	'B' => imagecolorallocate($im, 255, 51, 153),	// Dark Pink
	'C' => imagecolorallocate($im, 210, 180, 140),	// Tan
	'D' => imagecolorallocate($im, 127, 0, 255),	// Violet
	'E' => imagecolorallocate($im, 76, 0, 153),	// Deep Violet
	
	);

// map comes out upside-down
$lines = array_reverse($lines);

$assigned = array();
for ($x = 0; $x < $width; ++$x) {
	$assigned[$x] = array();
	for ($y = 0; $y < $height; ++$y) {
		$assigned[$x][$y] = FALSE;
	}
}

$y = 0;
foreach ($lines as $ll) {
	if (empty($ll)) {
		continue;
	}
	for ($x = 0; $x < strlen($ll); ++$x) {
		switch ($ll[$x]) {
			case '*': {		// start location
				if (!$assigned[$x][$y]) {
					imagesetpixel($im, $x, $y, $data[$ll[$x]]);
					$assigned[$x][$y] = TRUE;
				
					for ($hor = -3; $hor <= 3; ++$hor) {
						for ($ver = -3; $ver <= 3; ++$ver) {
							if ((abs($hor) + abs($ver)) <= 3) {
								$newx = min($width-1, max(0, $x+$hor));
								$newy = min($height-1, max(0, $y+$ver));
								if (!$assigned[$newx][$newy]) {
									imagesetpixel($im, $newx, $newy, $data[$ll[$x]]);
									$assigned[$newx][$newy] = TRUE;
								}
							}
						}
					}
				}
				break;
			}
			default: {
				if (!$assigned[$x][$y]) {
					imagesetpixel($im, $x, $y, $data[$ll[$x]]);
					// do not set assigned for default, let it be overwritten
				}
				break;
			}
		}
	}

	++$y;
}

// Set the content type header - in this case image/png
header('Content-Type: image/png');

// Output the image
imagepng($im, NULL, 9);

// Free up memory
imagedestroy($im);

/**/
