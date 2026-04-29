SET allow_experimental_gql_dialect = 1;
SET dialect = 'gql';

MATCH (n) WHERE n > 1 RETURN n LIMIT 1;
