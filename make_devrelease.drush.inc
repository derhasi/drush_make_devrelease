<?php

define('MAKE_DEVRELEASE_DEV_PATTERN', '/^(.+)(\+([0-9]+)-dev)$/is');

/**
 * Implements hook_drush_command().
 */
function make_devrelease_drush_command() {
  return array(
    'make-generatedev' => array(
      'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
      'description' => 'Updating make files with according git release information for dev releases.',
      'arguments' => array(
        'makefile' => 'Path to the makefile to update',
      ),
    ),
  );
}

/**
 * Drush command to update makefile with dev release information.
 *
 * @param string $makefile
 */
function drush_make_devrelease_make_generatedev($makefile) {

  $info = make_parse_info_file($makefile)
    + array('projects' => array(), 'libraries' => array());

  if (empty($info['api']) || $info['api'] != 2) {
    drush_log('The command only works with makefiles of API version 2.', 'error');
    return;
  }

  if (empty($info['core'])) {
    drush_log('No core version given in the makefile.', 'error');
    return;
  }

  // Alter project information with dev release information.
  _drush_make_devrelease_update_projects($info['projects'], $info['core']);

  // Write or print our makefile.
  // @see drush_make_generate()
  $contents = _drush_make_generate_makefile_contents($info['projects'], $info['libraries'], $info['core']);
  make_generate_print($contents, $makefile);
}

/**
 * Helper to update multiple project information.
 *
 * @param array $projects
 */
function _drush_make_devrelease_update_projects(&$projects, $core) {

  foreach ($projects as $name => $project) {
    if (empty($project['download']['type']) && _drush_make_devrelease_get_version($project, $core)) {
      $projects[$name] = _drush_make_devrelease_update_project_info($project, $name, $core);
    }
  }
}

/**
 * Helper to get the version information from a given project.
 *
 * @param array $project
 * @param string $core
 *
 * @return mixed
 */
function _drush_make_devrelease_get_version($project, $core) {
  if (isset($project['version'])) {
    return $project['version'];
  }
  elseif (isset($project['_devrelease'])) {
    return $project['_devrelease'];
  }
}

/**
 * Helper to update project information.
 *
 * @param array $project
 * @param string $name
 * @param string $core
 *
 * @return array
 *   The updated project information.
 */
function _drush_make_devrelease_update_project_info($project, $name, $core) {

  $matches = array();
  if (preg_match(MAKE_DEVRELEASE_DEV_PATTERN, $project['version'], $matches)) {
    list(, $release, , $offset) = $matches;

    // Keep original version number for reference.
    $project['_devrelease'] = $project['version'];

    if ($offset == 0) {
      $project['version'] = $release;
    }
    else {
      $revision = _drush_make_devrelease_get_revision($project, $name, $project['version'], $core, $release, $offset);
      if ($revision) {
        unset($project['version']);
        $project['download']['type'] = 'git';
        $project['download']['revision'] = $revision;
      }
    }
  }

  return $project;
}

/**
 * Helper to get a revision hash for a given project release with offset.
 *
 * @param array $project
 * @param string $name
 * @param string $version
 * @param string $core
 * @param string $release
 * @param string $offset
 *
 * @return string
 * @throws Exception
 */
function _drush_make_devrelease_get_revision($project, $name, $version, $core, $release, $offset) {
  drush_log(dt("Getting revision information for @name: @version (@core-@release)", array(
    '@name' => $name,
    '@version' => $version,
    '@core' => $core,
    '@release' => $release,
  )), 'status');

  $download = isset($project['download']) ? $project['download'] : array();

  $url = isset($download['url']) ? $download['url'] : "http://git.drupal.org/project/$name.git";
  $tmp_location = drush_tempdir() . '/devrelease-' . basename($name);

  $git = new MakeDevReleaseDrushGitController($tmp_location);
  $git->clone_url($url);

  // We extract tag from the version identifier and get the first branch that
  // contains this tag.
  // @todo: maybe we have to do some more work on getting the correct branch.
  $tag = "$core-$release";
  $branches = $git->exec('branch --contains=%s -r --no-track', $tag);

  // Check for all branches that hold the given
  foreach ($branches as $branch) {
    $branch = trim($branch);

    // Get all hashes for the given range. Using tag and branch syntax in rev-list
    // does not seem to work. So we grab the specific hashes to use those.
    $from = $git->exec('rev-list %s -1', $tag);
    $to = $git->exec('rev-list %s -1', $branch);
    $hashes = $git->exec('rev-list %s..%s --reverse', $from[0], $to[0]);

    // The "to" commit may be a valid revision too, in the case it is not the
    // "from" commit.
    if ($from[0] != $to[0]) {
      $hashes[] = $to[0];
    }

    // When we've got a valid hash for the given offset, we can download via git.
    $hash = $hashes[$offset];

    if (!empty($hash)) {
      break;
    }
  }

  // Remove the temp location again.
  drush_delete_tmp_dir($tmp_location);

  // Log error when there is no hash.
  if (empty($hash)) {
    drush_log(dt("No revision found for @name: @version (@core-@release)", array(
      '@name' => $name,
      '@version' => $version,
      '@core' => $core,
      '@release' => $release,
    )), 'error');
  }

  return $hash;
}

/**
 * Helper class to deal with a git repository.
 */
class MakeDevReleaseDrushGitController {

  /**
   * @var string
   */
  protected $location;

  /**
   * Constructor.
   *
   * @param string $location
   *   Path to initialise repository
   */
  public function __construct($location) {
    $this->location = $location;
  }

  /**
   * Clone repository to the given location.
   *
   * @param $url
   *
   * @return bool
   */
  public function clone_url($url) {
    $command = 'git clone %s %s';
    if (drush_get_context('DRUSH_VERBOSE')) {
      $command .= ' --verbose --progress';
    }

    // Before we can checkout anything, we need to clone the repository.
    if (!drush_shell_exec($command, $url, $this->location)) {
      throw new Exception('Could not clone url.');
    }
  }

  /**
   * Execute a git command.
   *
   * @param $command
   *   git command to execute. May hold sprintf-like placeholders (%s, ...).
   * @param ...
   *   Arguments to pass to the command as sprintf-like replacement values.
   *
   * @return array|bool
   */
  public function exec($command) {
    $args = func_get_args();
    $args[0] = 'git ' . $args[0];
    array_unshift($args, $this->location);

    call_user_func_array('drush_shell_cd_and_exec', $args);
    return drush_shell_exec_output();
  }
}

