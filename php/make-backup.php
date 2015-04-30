<?php

/**
* make-backup.php
* EmpireMUD 2.0 Backup Manager
* by Paul S. Clarke
*
* NOTE: You MUST update anything that says /path/to/ below.
*
* This program is meant to be run hourly and creates backups of the game.
* Hourly backups are kept for a day; daily backups are kept for a week; weekly
* backups are kept for a year. You need to run this from a backup directory
* outside of the mud itself, as it backs up the entire mud directory.
*
* If you have the disk space to spare, you should consider running this every
* hour on a crontab:
*    0 * * * * php /path/to/empiremud/php/make-backup.php
*
* Configuration options are near the top of the file.
*
* Based on this command: tar czf test.tgz --exclude='.*' --exclude='*.o' --exclude='bin/*' --exclude='lib/core.*' empireMUD
*
* @author Paul S. Clarke
* @version September 14, 2014
*/

 //////////////////////////////////////////////////////////////////////////////
//// CONFIGS /////////////////////////////////////////////////////////////////

$home_dir = '/path/to/home/dir/';	// the dir that CONTAINS your empiremud dir
$mud_dir = 'empireMUD';		// the name of your empiremud directory
$backup_dir = 'backups';	// the name of your backups directory
$exclude_files = array(
	'.*',	// files starting with .
	'*.o',	// compiled object files
	'bin/*',	// compiled binaries
	'lib/core.*',	// core dumps
);


 //////////////////////////////////////////////////////////////////////////////
//// MAKE BACKUP /////////////////////////////////////////////////////////////

chdir($home_dir);

$filename = $backup_dir . '/' . date('Y-m-d-H-i') . '.tgz';
$excludes = '';

foreach ($exclude_files as $file) {
	$excludes .= "--exclude='{$file}' ";
}

$command = "tar czf {$filename} {$excludes} {$mud_dir}";

// this should be system($command) but system is disabled and backticks aren't
`$command`;


 //////////////////////////////////////////////////////////////////////////////
//// MANAGE BACKUPS //////////////////////////////////////////////////////////

$hour_length = (60 * 60);
$day_length = ($hour_length * 24);
$week_length = ($day_length * 7);
$year_length = ($week_length * 52);

$hasDay = array();
$hasWeek = array();

$list = glob("{$backup_dir}/*.tgz");
foreach ($list as $file) {
	if (preg_match('/(\d+)-(\d+)-(\d+)-(\d+)-(\d+)/', $file, $match)) {
		$file_date = "{$match[1]}-{$match[2]}-{$match[3]} {$match[4]}:{$match[5]}:00";
		$timestamp = strtotime($file_date);
		
		$diff = time() - $timestamp;
		
		if ($diff > $year_length) {
			unlink($file);
		}
		else if ($diff > $week_length) {
			$week = floor($diff / ($week_length-$hour_length));
			if (!isset($hasWeek[$week])) {
				$hasWeek[$week] = TRUE;
			}
			else {
				unlink($file);
			}
		}
		else if ($diff > $day_length) {
			$day = floor($diff / ($day_length-$hour_length));
			if (!isset($hasDay[$day])) {
				$hasDay[$day] = TRUE;
			}
			else {
				unlink($file);
			}
		}
	}
}
