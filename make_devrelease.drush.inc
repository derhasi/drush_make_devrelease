<?php

/**
 * Implements hook_drush_command().
 */
function make_devrelease_drush_command() {
  return array();
}
/**
 * Implements make_download_TYPE().
 */
function make_download_devrelease($name, $type, $download, $download_location) {

  // If the version holds a dev definition, we try to get the corresponding git
  // commit for that version, as the pm approach does not support this.
  $pattern_stable = '/^(.+)-(.+)$/is';
  $pattern_dev = '/^(.+)-(.+)(\+([0-9]+)-dev)$/is';

  $matches = array();

  $offset = 0;
  if (preg_match($pattern_dev, $download['version'], $matches)) {
    list( , $core, $release, , $offset) = $matches;
  }
  elseif (preg_match($pattern_stable, $download['version'], $matches)) {
    list( , $core, $release) = $matches;
  }

  $url = isset($download['url']) ? $download['url'] : "http://git.drupal.org/project/$name.git";
  $tmp_location = drush_tempdir() . '/devrelease-' . basename($download_location);

  $git = new MakeDevReleaseDrushGitController($tmp_location);
  $git->clone_url($url);

  // We extract tag from the version identifier and get the first branch that
  // contains this tag.
  // @todo: maybe we have to do some more work on getting the correct branch.
  $tag = "$core-$release";
  $branches = $git->exec('branch --contains=%s -r --no-track', $tag);
  $branch = trim($branches[0]);

  // Get all hashes for the given range. Using tag and branch syntax in rev-list
  // does not seem to work. So we grab the specific hashes to use those.
  $from = $git->exec('rev-list %s -1', $tag);
  $to = $git->exec('rev-list %s -1', $branch);
  $hashes = $git->exec('rev-list %s..%s --reverse', $from[0], $to[0]);

  // When we've got a valid hash for the given offset, we can download via git.
  $hash = $hashes[$offset];

  if ($hash) {
    return make_download_git($name, $type, array(
      'url' => $url,
      'revision' => $hash,
    ), $download_location);
  }

  return FALSE;
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

