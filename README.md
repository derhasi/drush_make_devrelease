# make_devrelease for drush

Drush plugin for updating an existing make file with git information for drupal
dev releases.

## Command

```
drush make-generatedev [makefile.make]
```

## Example

### Before:

```
projects[wysiwyg][subdir] = "contrib"
projects[wysiwyg][version] = "2.2+23-dev"
```

### After:

```
projects[wysiwyg][subdir] = "contrib"
projects[wysiwyg][_devrelease] = "2.2+23-dev"
projects[wysiwyg][download][type] = "git"
projects[wysiwyg][download][revision] = "40b26a15d3f79170e25bdef105620edf5c0f8892"
```

