package ag.boersego.v8annotations;

/**
 * Created by martin on 20.12.17.
 */

public enum V8Symbols {
    /*
     * Default value; not an actual symbol
     */
    NONE,

    // Iteration symbols

    /*
     * A method returning the default iterator for an object. Used by for...of.
     */
    ITERATOR,
    /*
     * A method that returns the default AsyncIterator for an object. Used by for await of.
     */
    ASYNC_ITERATOR,

    // Regular expression symbols

    /*
    * A method that matches against a string, also used to determine if an object may be used as a regular expression. Used by String.prototype.match().
    */
    MATCH,
    /*
    * A method that replaces matched substrings of a string. Used by String.prototype.replace().
     */
    REPLACE,
    /*
    * A method that returns the index within a string that matches the regular expression. Used by String.prototype.search().
    */
    SEARCH,
    /*
    * A method that splits a string at the indices that match a regular expression. Used by String.prototype.split().
    */
    SPLIT,

    // Other symbols

    /*
    * A method determining if a constructor object recognizes an object as its instance. Used by instanceof.
    */
    HAS_INSTANCE,
    /*
    * A Boolean value indicating if an object should be flattened to its array elements. Used by Array.prototype.concat().
    */
    IS_CONCAT_SPREADABLE,
    /*
    * An object value of whose own and inherited property names are excluded from the with environment bindings of the associated object.
    */
    UNSCOPABLES,
    /*
    * A method converting an object to a primitive value.
    */
    TO_PRIMITIVE,
    /*
    * A string value used for the default description of an object. Used by Object.prototype.toString().
    */
    TO_STRING_TAG
}