package ag.boersego.v8annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Created by martin on 02.10.17.
 */

@Retention(RetentionPolicy.SOURCE)
@Target(ElementType.METHOD)
public @interface V8Getter {
    String property() default "";
}
