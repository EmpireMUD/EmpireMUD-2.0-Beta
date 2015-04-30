<?php
/**
* map-thumb-generator.php
* EmpireMUD 2.0 thumbnail generator
* by Paul S. Clarke
*
* NOTE: You MUST update anything that says /path/to/ below.
*
* This file turns your already-generated map images into smaller thumbnails
* suitable for using with map-show.php.
*
* Using this file:
*  1. you must generator map png images using map.php first
*  2. update the $source/$dest variables near the top of this file
*  3. run this periodically to generate map thumbs, or automate it with
*     a crontab entry like this:
*     20 0 * * * php /path/to/empiremud/php/map-thumb-generator.php
*
* This creates images with the format map-1-01-2015.png
*/

// CONFIGURE THESE:
// absolute path to your live map image
$source = '/path/to/html/docroot/map.png';
// absolute path to the directory to save thumbnails
$dest = '/path/to/html/docroot/old-maps/';
// size you want the images (height is proportionate)
$newWidth = 900;


// work
$im = imagecreatefrompng($source);

// no work
if (!$im) {
	exit;
}

list($width, $height) = getimagesize($source);
$newHeight = $height * $newWidth / $width;

$thumb = imagecreatetruecolor($newWidth, $newHeight);
imagecopyresized($thumb, $im, 0, 0, 0, 0, $newWidth, $newHeight, $width, $height);

// Output the image
$filename = $dest . sprintf('map-%s.png', date('n-d-Y'));
imagepng($thumb, $filename, 9);
