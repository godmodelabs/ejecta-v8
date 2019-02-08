package ag.boersego.v8annotations;

/**
 * Created by martin on 20.12.17.
 */

public enum V8Symbols {
    /*
     * Default value; not an actual symbol
     */
    NONE,
    /*
     * A method returning the default iterator for an object. Used by for...of.
     */
    ITERATOR,
    /*
     * A method that returns the default AsyncIterator for an object. Used by for await of.
     */
    ASYNC_ITERATOR
};