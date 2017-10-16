package ag.boersego.v8annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Created by martin on 16.10.17.
 */
@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.TYPE)
public @interface V8Class {
    V8ClassCreationPolicy creationPolicy() default V8ClassCreationPolicy.DEFAULT;
}
