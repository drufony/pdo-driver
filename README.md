PDO Driver for Drupal 7
=======================

Instead of allowing Drupal to construct your PDO objects, do it yourself -- the hard way!

Why
---

When you have a PDO object, you can inject it into other services. While the Drupal 7
`\DatabaseConnection` class extends `\PDO`, it changes some defaults that make the
connection object less usable elsewhere. With this driver, the same PDO object can be
used in Drupal 7 and in DBAL.

Example
-------

In `settings.php`, configure database with the `pdo` driver. Then add a `pdo` property
which is your `\PDO` object.

```php
$databases = array (
  'default' => 
  array (
    'default' => 
    array (
      'driver' => 'pdo',
      'pdo' => new \PDO('mysql:host=localhost;port=3306;dbname=drupal', 'root', '', array(
        PDO::MYSQL_ATTR_USE_BUFFERED_QUERY => TRUE,
        PDO::ATTR_EMULATE_PREPARES => TRUE,
      )),
      'prefix' => '',
    ),
  ),
);
```
