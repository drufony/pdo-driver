<?php

/**
 * @file
 * Database interface code for PDO database servers.
 */

/**
 * @addtogroup database
 * @{
 */
class DatabaseConnection_pdo extends DatabaseConnection
{
    /** @var  \PDO */
    protected $pdo;

    /** @var  \DatabaseHelper */
    protected $helper;

    public function __construct(array $connection_options = array())
    {
        $this->connectionOptions = $connection_options;

        // Initialize and prepare the connection prefix.
        $this->setPrefix(isset($this->connectionOptions['prefix']) ? $this->connectionOptions['prefix'] : '');

        // Because the other methods don't seem to work right.
        $driver_options[PDO::ATTR_ERRMODE] = PDO::ERRMODE_EXCEPTION;

        // Call PDO::__construct and PDO::setAttribute.
        $this->pdo = $connection_options['pdo'];

        $helper_class = 'DatabaseHelper_'. $this->driver();
        $this->helper = new $helper_class($this);

        // Set a Statement class, unless the driver opted out.
        if (!empty($this->statementClass)) {
            $this->setAttribute(PDO::ATTR_STATEMENT_CLASS, array($this->statementClass, array($this)));
        }
    }

    /**
     * {@inheritdocs}
     */
    public function prepareQuery($query) {
        $query = $this->prefixTables($query);

        // Call PDO::prepare.
        return $this->prepare($query);
    }

    /**
     * {@inheritdocs}
     */
    public function queryRange(
      $query,
      $from,
      $count,
      array $args = array(),
      array $options = array()
    ) {
        return $this->helper->queryRange($query, $from, $count, $args, $options);
    }

    /**
     * {@inheritdocs}
     */
    function queryTemporary(
      $query,
      array $args = array(),
      array $options = array()
    ) {
        return $this->helper->queryTemporary($query, $args, $options);
    }

    /**
     * {@inheritdocs}
     */
    public function driver()
    {
        return $this->pdo->getAttribute(\PDO::ATTR_DRIVER_NAME);
    }

    /**
     * {@inheritdocs}
     */
    public function databaseType()
    {
        return $this->pdo->getAttribute(\PDO::ATTR_DRIVER_NAME);
    }

    /**
     * {@inheritdocs}
     */
    public function mapConditionOperator($operator)
    {
        return $this->helper->mapConditionOperator($operator);
    }

    /**
     * {@inheritdocs}
     */
    public function nextId($existing_id = 0)
    {
        return $this->helper->nextId($existing_id);
    }

    /**
     * {@inheritdocs}
     */
    public function prepare($statement, $driver_options = array())
    {
        return $this->pdo->prepare($statement, $driver_options);
    }

    /**
     * {@inheritdocs}
     */
    public function beginTransaction()
    {
        return $this->pdo->beginTransaction();
    }

    /**
     * {@inheritdocs}
     */
    public function commit()
    {
        return $this->pdo->commit();
    }

    /**
     * {@inheritdocs}
     */
    public function rollback($savepoint_name = 'drupal_transaction')
    {
        if (!$this->supportsTransactions()) {
            return;
        }
        if (!$this->inTransaction()) {
            throw new DatabaseTransactionNoActiveException();
        }
        // A previous rollback to an earlier savepoint may mean that the savepoint
        // in question has already been accidentally committed.
        if (!isset($this->transactionLayers[$savepoint_name])) {
            throw new DatabaseTransactionNoActiveException();
        }

        // We need to find the point we're rolling back to, all other savepoints
        // before are no longer needed. If we rolled back other active savepoints,
        // we need to throw an exception.
        $rolled_back_other_active_savepoints = FALSE;
        while ($savepoint = array_pop($this->transactionLayers)) {
            if ($savepoint == $savepoint_name) {
                // If it is the last the transaction in the stack, then it is not a
                // savepoint, it is the transaction itself so we will need to roll back
                // the transaction rather than a savepoint.
                if (empty($this->transactionLayers)) {
                    break;
                }
                $this->query('ROLLBACK TO SAVEPOINT ' . $savepoint);
                $this->popCommittableTransactions();
                if ($rolled_back_other_active_savepoints) {
                    throw new DatabaseTransactionOutOfOrderException();
                }
                return;
            }
            else {
                $rolled_back_other_active_savepoints = TRUE;
            }
        }
        $this->pdo->rollBack();
        if ($rolled_back_other_active_savepoints) {
            throw new DatabaseTransactionOutOfOrderException();
        }
    }

    /**
     * {@inheritdocs}
     */
    public function pushTransaction($name) {
        if (!$this->supportsTransactions()) {
            return;
        }
        if (isset($this->transactionLayers[$name])) {
            throw new DatabaseTransactionNameNonUniqueException($name . " is already in use.");
        }
        // If we're already in a transaction then we want to create a savepoint
        // rather than try to create another transaction.
        if ($this->inTransaction()) {
            $this->pdo->query('SAVEPOINT ' . $name);
        }
        else {
            $this->pdo->beginTransaction();
        }
        $this->transactionLayers[$name] = $name;
    }

    /**
     * {@inheritdocs}
     */
    public function inTransaction()
    {
        return $this->pdo->inTransaction();
    }

    /**
     * {@inheritdocs}
     */
    public function setAttribute($attribute, $value)
    {
        return $this->pdo->setAttribute($attribute, $value);
    }

    /**
     * {@inheritdocs}
     */
    public function exec($statement)
    {
        return $this->pdo->exec($statement);
    }

    /**
     * {@inheritdocs}
     */
    public function lastInsertId($name = null)
    {
        return $this->pdo->lastInsertId($name);
    }

    /**
     * {@inheritdocs}
     */
    public function errorCode()
    {
        return $this->pdo->errorCode();
    }

    /**
     * {@inheritdocs}
     */
    public function errorInfo()
    {
        return $this->pdo->errorInfo();
    }

    /**
     * {@inheritdocs}
     */
    public function getAttribute($attribute)
    {
        return $this->pdo->getAttribute($attribute);
    }

    /**
     * {@inheritdocs}
     */
    public function quote($string, $parameter_type = PDO::PARAM_STR)
    {
        return $this->pdo->quote($string, $parameter_type);
    }

    /**
     * {@inheritdocs}
     */
    protected function popCommittableTransactions()
    {
        if ($this->helper instanceof DatabaseTransactionHelper) {
            $this->helper->popCommittableTransactions($this->transactionLayers);
        }
        else {
            parent::popCommittableTransactions();
        }
    }
}

interface DatabaseHelper
{
    public function __construct(DatabaseConnection_pdo $conn);
    public function queryRange($query, $from, $count, array $args = array(), array $options = array());
    public function queryTemporary($query, array $args = array(), array $options = array());
    public function mapConditionOperator($operator);
    public function nextId($existing_id = 0);
}

interface DatabaseTransactionHelper
{
    public function popCommittableTransactions(&$transactionLayers);
}

abstract class DatabaseHelper_pdo implements DatabaseHelper
{
    /** @var DatabaseConnection_pdo */
    protected $conn;

    /**
     * An index used to generate unique temporary table names.
     *
     * @var integer
     */
    protected $temporaryNameIndex = 0;

    public function __construct(\DatabaseConnection_pdo $conn)
    {
        $this->conn = $conn;
    }

    /**
     * Generates a temporary table name.
     *
     * @return
     *   A table name.
     */
    protected function generateTemporaryTableName() {
        return "db_temporary_" . $this->temporaryNameIndex++;
    }
}

class DatabaseHelper_mysql extends DatabaseHelper_pdo implements DatabaseTransactionHelper
{
    /**
     * Flag to indicate if the cleanup function in __destruct() should run.
     *
     * @var boolean
     */
    protected $needsCleanup = FALSE;

    public function queryRange(
      $query,
      $from,
      $count,
      array $args = array(),
      array $options = array()
    ) {
        return $this->conn->query($query . ' LIMIT ' . (int) $from . ', ' . (int) $count, $args, $options);
    }

    public function __destruct() {
        if ($this->needsCleanup) {
            $this->nextIdDelete();
        }
    }

    public function queryTemporary(
      $query,
      array $args = array(),
      array $options = array()
    ) {
        $tablename = $this->generateTemporaryTableName();
        $this->conn->query('CREATE TEMPORARY TABLE {' . $tablename . '} Engine=MEMORY ' . $query, $args, $options);
        return $tablename;
    }

    public function mapConditionOperator($operator)
    {
        // We don't want to override any of the defaults.
        return NULL;
    }

    public function nextId($existing_id = 0)
    {
        $new_id = $this->conn->query('INSERT INTO {sequences} () VALUES ()', array(), array('return' => Database::RETURN_INSERT_ID));
        // This should only happen after an import or similar event.
        if ($existing_id >= $new_id) {
            // If we INSERT a value manually into the sequences table, on the next
            // INSERT, MySQL will generate a larger value. However, there is no way
            // of knowing whether this value already exists in the table. MySQL
            // provides an INSERT IGNORE which would work, but that can mask problems
            // other than duplicate keys. Instead, we use INSERT ... ON DUPLICATE KEY
            // UPDATE in such a way that the UPDATE does not do anything. This way,
            // duplicate keys do not generate errors but everything else does.
            $this->conn->query('INSERT INTO {sequences} (value) VALUES (:value) ON DUPLICATE KEY UPDATE value = value', array(':value' => $existing_id));
            $new_id = $this->conn->query('INSERT INTO {sequences} () VALUES ()', array(), array('return' => Database::RETURN_INSERT_ID));
        }
        $this->needsCleanup = TRUE;
        return $new_id;
    }

    public function nextIdDelete() {
        // While we want to clean up the table to keep it up from occupying too
        // much storage and memory, we must keep the highest value in the table
        // because InnoDB  uses an in-memory auto-increment counter as long as the
        // server runs. When the server is stopped and restarted, InnoDB
        // reinitializes the counter for each table for the first INSERT to the
        // table based solely on values from the table so deleting all values would
        // be a problem in this case. Also, TRUNCATE resets the auto increment
        // counter.
        try {
            $max_id = $this->conn->query('SELECT MAX(value) FROM {sequences}')->fetchField();
            // We know we are using MySQL here, no need for the slower db_delete().
            $this->conn->query('DELETE FROM {sequences} WHERE value < :value', array(':value' => $max_id));
        }
            // During testing, this function is called from shutdown with the
            // simpletest prefix stored in $this->connection, and those tables are gone
            // by the time shutdown is called so we need to ignore the database
            // errors. There is no problem with completely ignoring errors here: if
            // these queries fail, the sequence will work just fine, just use a bit
            // more database storage and memory.
        catch (PDOException $e) {
        }
    }

    /**
     * Overridden to work around issues to MySQL not supporting transactional DDL.
     * @param $transactionLayers
     * @throws DatabaseTransactionCommitFailedException
     */
    public function popCommittableTransactions(&$transactionLayers) {
        // Commit all the committable layers.
        foreach (array_reverse($transactionLayers) as $name => $active) {
            // Stop once we found an active transaction.
            if ($active) {
                break;
            }

            // If there are no more layers left then we should commit.
            unset($transactionLayers[$name]);
            if (empty($transactionLayers)) {
                if (!$this->conn->commit()) {
                    throw new DatabaseTransactionCommitFailedException();
                }
            }
            else {
                // Attempt to release this savepoint in the standard way.
                try {
                    $this->conn->query('RELEASE SAVEPOINT ' . $name);
                }
                catch (PDOException $e) {
                    // However, in MySQL (InnoDB), savepoints are automatically committed
                    // when tables are altered or created (DDL transactions are not
                    // supported). This can cause exceptions due to trying to release
                    // savepoints which no longer exist.
                    //
                    // To avoid exceptions when no actual error has occurred, we silently
                    // succeed for MySQL error code 1305 ("SAVEPOINT does not exist").
                    if ($e->errorInfo[1] == '1305') {
                        // If one SAVEPOINT was released automatically, then all were.
                        // Therefore, clean the transaction stack.
                        $transactionLayers = array();
                        // We also have to explain to PDO that the transaction stack has
                        // been cleaned-up.
                        $this->conn->commit();
                    }
                    else {
                        throw $e;
                    }
                }
            }
        }
    }
}

/**
 * @} End of "addtogroup database".
 */
