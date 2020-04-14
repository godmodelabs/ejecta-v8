package ag.boersego.v8annotations.compiler;

import java.io.IOException;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.RoundEnvironment;
import javax.annotation.processing.SupportedAnnotationTypes;
import javax.annotation.processing.SupportedSourceVersion;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.Element;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.TypeElement;
import javax.lang.model.element.VariableElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.ExecutableType;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.lang.model.util.Elements;
import javax.lang.model.util.Types;
import javax.tools.Diagnostic;
import javax.tools.JavaFileObject;

import ag.boersego.v8annotations.*;

@SupportedAnnotationTypes(
        {
                "ag.boersego.v8annotations.V8Class",
                "ag.boersego.v8annotations.V8Function",
                "ag.boersego.v8annotations.V8Getter",
                "ag.boersego.v8annotations.V8Setter",
                "ag.boersego.v8annotations.V8UndefinedIsNull"
        }
)
@SupportedSourceVersion(SourceVersion.RELEASE_8)
public final class V8AnnotationProcessor extends AbstractProcessor {

    private static TypeMirror sBooleanBox, sCharBox, sLongBox, sIntBox, sFloatBox, sDoubleBox, sShortBox, sByteBox;
    private static TypeMirror sObjectType, sStringType;
    private static TypeMirror sV8FunctionType, sV8GenericObjectType, sV8ObjectType, sV8ArrayType, sV8UndefinedType;

    private static class V8Nullable {
        boolean nullable;
        boolean undefinedIsNull;
    }

    private static class AccessorTuple extends V8Nullable {
        String property;
        Element getter;
        Element setter;
        TypeMirror kind;
    }

    private static class AnnotationHolder {
        private TypeElement classElement;
        private ArrayList<AnnotatedFunctionHolder> annotatedFunctions;
        private ArrayList<AccessorTuple> annotatedAccessors;
        private boolean createFromJavaOnly;
    }

    private static class AnnotatedFunctionHolder {
        private final String returnType;
        private ArrayList<AnnotatedFunctionParamHolder> params;
        private final Element element;

        public AnnotatedFunctionHolder(final Element element, String returnType) {
            this.element = element;
            this.returnType = returnType;
        }

        public void makeParameters() {
            params = new ArrayList<>();
        }

        public void addParameter(final AnnotatedFunctionParamHolder holder) {
            params.add(holder);
        }
    }

    private static class AnnotatedFunctionParamHolder extends V8Nullable {

        private String type;

        public AnnotatedFunctionParamHolder(String paramType) {
            type = paramType;
        }
    }

    //Suppress warning for "TypeElement.class.cast(element)" because "(TypeElement) element" cast mysteriously doesn't compile
    @SuppressWarnings("RedundantClassCall")
    private AnnotationHolder getHolder(HashMap<String, AnnotationHolder> annotatedClasses, Element element) {
        TypeElement classElement;
        if (element.getKind().isClass()) {
            classElement = TypeElement.class.cast(element);
        } else {
            classElement = TypeElement.class.cast(element.getEnclosingElement());
        }
        String name = classElement.getQualifiedName().toString();

        AnnotationHolder holder;
        holder = annotatedClasses.get(name);
        if (holder == null) {
            holder = new AnnotationHolder();
            holder.annotatedFunctions = new ArrayList<>();
            holder.annotatedAccessors = new ArrayList<>();
            holder.classElement = classElement;
            holder.createFromJavaOnly = false;
            annotatedClasses.put(name, holder);
        }

        return holder;
    }

    private AccessorTuple getAccessorTuple(HashMap<String, AnnotationHolder> annotatedClasses, Element element, String property) {
        AnnotationHolder holder = getHolder(annotatedClasses, element);
        AccessorTuple tuple = null;

        for (AccessorTuple t : holder.annotatedAccessors) {
            if (t.property.equals(property)) {
                tuple = t;
                break;
            }
        }
        if (tuple == null) {
            tuple = new AccessorTuple();
            tuple.property = property;
            holder.annotatedAccessors.add(tuple);
        }
        return tuple;
    }

    private String lcfirst(String string) {
        char[] c = string.toCharArray();
        c[0] = Character.toLowerCase(c[0]);
        return new String(c);
    }

    private void generateBinding(AnnotationHolder holder) {
        String className = holder.classElement.getQualifiedName().toString();
        String generatedClassName = className + "$V8Binding";

        String pkg = generatedClassName.substring(0, className.lastIndexOf("."));
        String name = generatedClassName.substring(className.lastIndexOf(".") + 1);

        long index;

        StringBuilder builder = new StringBuilder()
                .append("package " + pkg + ";\n\n")
                .append("import ag.boersego.v8annotations.generated.V8FunctionInfo;\n")
                .append("import ag.boersego.v8annotations.generated.V8AccessorInfo;\n\n")
                .append("public class ").append(name).append(" {\n\n") // open class
                .append("\tpublic static boolean createFromJavaOnly = ").append(holder.createFromJavaOnly ? "true" : "false").append(";\n\n")
                .append("\tpublic static V8FunctionInfo[] getV8Functions() {\n") // open method
                .append("\t\tV8FunctionInfo[] res = {\n");

        index = 0;
        for (final AnnotatedFunctionHolder functionHolder : holder.annotatedFunctions) {
            final Element e = functionHolder.element;
            String methodName = e.getSimpleName().toString();
            String property;
            V8Symbols symbol = e.getAnnotation(V8Function.class).symbol();
            if(symbol != V8Symbols.NONE) {
                property = "symbol:" + symbol.toString();
            } else {
                property = e.getAnnotation(V8Function.class).property();
                if (property.isEmpty()) {
                    property = methodName;
                }
                property = "string:" + property;
            }
            boolean isStatic = e.getModifiers().contains(Modifier.STATIC);
            builder.append("\t\t\t").append(index++ == 0 ? "" : ",")
                    .append("new V8FunctionInfo(\"")
                    .append(property)
                    .append("\", \"")
                    .append(methodName)
                    .append("\", \"")
                    .append(functionHolder.returnType)
                    .append("\", ")
                    .append(isStatic ? "true" : "false");

            if(functionHolder.params != null) {
                builder.append(", new V8FunctionInfo.V8FunctionArgumentInfo[] {");

                int paramIndex = 0;
                for (final AnnotatedFunctionParamHolder paramHolder : functionHolder.params) {
                    if (paramIndex++ != 0) {
                        builder.append(",");
                    }
                    builder.append("\n\t\t\t\tnew V8FunctionInfo.V8FunctionArgumentInfo(\"")
                            .append(paramHolder.type)
                            .append("\", ")
                            .append(paramHolder.nullable)
                            .append(", ")
                            .append(paramHolder.undefinedIsNull)
                            .append(")");
                }

                if (paramIndex != 0) {
                    builder.append("\n\t\t\t");
                }
                builder.append("}");
            } else {
                builder.append(", null");
            }

            builder.append(")\n");
        }

        builder.append("\t\t};\n")
                .append("\t\treturn res;\n")
                .append("\t}\n\n") // close method
                .append("\tpublic static V8AccessorInfo[] getV8Accessors() {\n") // open method
                .append("\t\tV8AccessorInfo[] res = {\n");

        index = 0;
        for (AccessorTuple tuple : holder.annotatedAccessors) {
            String getterName = tuple.getter != null ? tuple.getter.getSimpleName().toString() : null;
            String setterName = tuple.setter != null ? tuple.setter.getSimpleName().toString() : null;
            String typeCode = null;
            if (tuple.kind != null) {
                typeCode = getJniCodeForType(tuple.getter, tuple.kind, false);
            }
            String typeName = tuple.kind != null ? tuple.kind.toString() : null;
            boolean isGetterStatic = tuple.getter != null && tuple.getter.getModifiers().contains(Modifier.STATIC);
            boolean isSetterStatic = tuple.setter != null && tuple.setter.getModifiers().contains(Modifier.STATIC);
            if (tuple.getter != null && tuple.setter != null && isGetterStatic != isSetterStatic) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "getter and setter must both have same receiver type (static or non-static)", tuple.getter);
            }
            builder.append("\t\t\t")
                    .append(index++ == 0 ? "" : ",")
                    .append("new V8AccessorInfo(\"")
                    .append(tuple.property)
                    .append("\", ");
            if (typeCode != null) {
                builder.append("\"").append(typeCode).append("\", ");
            } else {
                builder.append("null, ");
            }
            if (getterName != null) {
                builder.append("\"").append(getterName).append("\", ");
            } else {
                builder.append("null, ");
            }
            if (setterName != null) {
                builder.append("\"").append(setterName).append("\", ");
            } else {
                builder.append("null, ");
            }

            builder.append(isGetterStatic ? "true, " : "false, ").
                    append(tuple.nullable ? "true, " : "false, ").
                    append(tuple.undefinedIsNull ? "true" : "false").
                    append(")\n");
        }

        builder.append("\t\t};\n")
                .append("\t\treturn res;\n")
                .append("\t}\n\n") // close method
                .append("}\n"); // close class

        try { // write the file
            JavaFileObject source = processingEnv.getFiler().createSourceFile(generatedClassName);

            Writer writer = source.openWriter();
            writer.write(builder.toString());
            writer.flush();
            writer.close();
        } catch (IOException e) {
            // Note: calling e.printStackTrace() will print IO errors
            // that occur from the file already existing after its first run, this is normal
        }
    }


    /*---------------
     * Helper methods borrowed from Dagger
     */

    @Override
    public boolean process(
            Set<? extends TypeElement> annotations,
            RoundEnvironment env) {

        HashMap<String, AnnotationHolder> annotatedClasses = new HashMap<>();

        Elements elems = processingEnv.getElementUtils();
        Types types = processingEnv.getTypeUtils();

        if (sBooleanBox == null) {
            sObjectType = elems.getTypeElement("java.lang.Object").asType();
            sStringType = elems.getTypeElement("java.lang.String").asType();
            sBooleanBox = elems.getTypeElement("java.lang.Boolean").asType();
            sByteBox = elems.getTypeElement("java.lang.Byte").asType();
            sCharBox = elems.getTypeElement("java.lang.Character").asType();
            sShortBox = elems.getTypeElement("java.lang.Short").asType();
            sIntBox = elems.getTypeElement("java.lang.Integer").asType();
            sLongBox = elems.getTypeElement("java.lang.Long").asType();
            sFloatBox = elems.getTypeElement("java.lang.Float").asType();
            sDoubleBox = elems.getTypeElement("java.lang.Double").asType();
            sV8FunctionType = elems.getTypeElement("ag.boersego.bgjs.JNIV8Function").asType();
            sV8ObjectType = elems.getTypeElement("ag.boersego.bgjs.JNIV8Object").asType();
            sV8GenericObjectType = elems.getTypeElement("ag.boersego.bgjs.JNIV8GenericObject").asType();
            sV8ArrayType = elems.getTypeElement("ag.boersego.bgjs.JNIV8Array").asType();
        }
        ArrayType objectArrayType = types.getArrayType(sObjectType);

        for (Element element : env.getElementsAnnotatedWith(V8Class.class)) {
            AnnotationHolder holder = getHolder(annotatedClasses, element);
            holder.createFromJavaOnly = (element.getAnnotation(V8Class.class).creationPolicy() == V8ClassCreationPolicy.JAVA_ONLY);
        }
        for (Element element : env.getElementsAnnotatedWith(V8Function.class)) {
            // validate signature
            ExecutableType emeth = (ExecutableType) element.asType();

            final String returnType = getJniCodeForType(element, emeth.getReturnType(), true);
            final AnnotatedFunctionHolder functionHolder = new AnnotatedFunctionHolder(element, returnType);

            List<? extends TypeMirror> functionParams = emeth.getParameterTypes();

            boolean isGenericMethod = functionParams.size() == 1 &&
                    types.isSameType(functionParams.get(0), objectArrayType) &&
                    types.isSameType(emeth.getReturnType(), sObjectType);

            if(!isGenericMethod) {
                functionHolder.makeParameters();
                for (final TypeMirror param : functionParams) {
                    if (validateAccessorType(element, param)) {
                        final String paramType = getJniCodeForType(element, param, false);
                        final AnnotatedFunctionParamHolder holder = new AnnotatedFunctionParamHolder(paramType);
                        parseAccessorNullable(holder, element, param);
                        functionHolder.addParameter(holder);
                    }
                }
            }

            // store
            AnnotationHolder holder = getHolder(annotatedClasses, element);

            holder.annotatedFunctions.add(functionHolder);
        }
        TypeMirror getterKind;
        for (Element element : env.getElementsAnnotatedWith(V8Getter.class)) {
            // determine property name
            String property;
            V8Symbols symbol = element.getAnnotation(V8Getter.class).symbol();
            if(symbol != V8Symbols.NONE) {
                property = "symbol:" + symbol.toString();
            } else {
                property = element.getAnnotation(V8Getter.class).property();
                if (property.isEmpty()) {
                    String name = element.getSimpleName().toString();
                    if (name.indexOf("get") != 0 && name.indexOf("is") != 0) {
                        processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "no property name specified and method does not start with 'get' or 'is'", element);
                    }
                    property = lcfirst(name.indexOf("get") == 0 ? name.substring(3) : name.substring(2));
                }
                property = "string:" + property;
            }
            // validate signature
            ExecutableType emeth = (ExecutableType) element.asType();
            getterKind = emeth.getReturnType();
            if (emeth.getParameterTypes().size() != 0) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "annotated method must not require parameters", element);
            }
            // store
            AccessorTuple tuple = getAccessorTuple(annotatedClasses, element, property);
            tuple.getter = element;
            if (validateAccessorType(element, getterKind)) {
                tuple.kind = getterKind;
            }
        }
        TypeMirror setterKind;
        for (Element element : env.getElementsAnnotatedWith(V8Setter.class)) {
            // determine property name
            String property;
            V8Symbols symbol = element.getAnnotation(V8Setter.class).symbol();
            if(symbol != V8Symbols.NONE) {
                property = "symbol:" + symbol.toString();
            } else {
                property = element.getAnnotation(V8Setter.class).property();
                if (property.isEmpty()) {
                    String name = element.getSimpleName().toString();
                    if (name.indexOf("set") != 0) {
                        processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "no property name specified and method does not start with 'set'", element);
                    }
                    property = lcfirst(name.substring(3));
                }
                property = "string:" + property;
            }
            AccessorTuple tuple = getAccessorTuple(annotatedClasses, element, property);

            // validate signature
            ExecutableType emeth = (ExecutableType) element.asType();
            if (!emeth.getReturnType().getKind().equals(TypeKind.VOID)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "annotated method must have return type void", element);
            }
            if (emeth.getParameterTypes().size() != 1 || tuple.kind != null && !types.isSameType(emeth.getParameterTypes().get(0), tuple.kind)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "annotated method must accept exactly one parameter of type " + tuple.kind, element);
            }
            setterKind = emeth.getParameterTypes().get(0);

            // store
            if (validateAccessorType(element, setterKind)) {
                parseAccessorNullable(tuple, element, setterKind);
                if (tuple.nullable) {
                    // @TODO: something missing here?
                }
            }
            if(tuple.kind == null) {
                tuple.kind = setterKind;
            }
            tuple.setter = element;
        }
        for (String key : annotatedClasses.keySet()) {
            AnnotationHolder holder = annotatedClasses.get(key);
            generateBinding(holder);
        }

        return true;
    }

    private boolean parseAccessorNullable(final V8Nullable tuple, final Element element, final TypeMirror mirror) {
        if (mirror.getKind().isPrimitive()) {
            tuple.nullable = false;
            return false;
        }

        boolean undefinedIsNull = false;
        VariableElement param = ((ExecutableElement) element).getParameters().get(0);
        List<? extends AnnotationMirror> mirrors = param.getAnnotationMirrors();
        for (AnnotationMirror annotation : mirrors) {
            final String annotationName = annotation.toString();
            // processingEnv.getMessager().printMessage(Diagnostic.Kind.MANDATORY_WARNING, "Annotationmirror is " + annotationName, element);
            if (annotationName.equals("@ag.boersego.v8annotations.V8UndefinedIsNull")) {
                undefinedIsNull = true;
                // processingEnv.getMessager().printMessage(Diagnostic.Kind.MANDATORY_WARNING, "undefined is null", element);
                continue;
            }
            if (annotationName.endsWith(".NotNull")) {
                tuple.nullable = false;
                return false;
            }
            if (annotationName.endsWith(".Nullable")) {
                tuple.nullable = true;
            }
        }

        tuple.nullable = true;

        if (!undefinedIsNull) {
            // processingEnv.getMessager().printMessage(Diagnostic.Kind.MANDATORY_WARNING, "nullable annotation is " + element.getAnnotation(V8UndefinedIsNull.class), element);
            undefinedIsNull = element.getAnnotation(V8UndefinedIsNull.class) != null;
        }

        tuple.undefinedIsNull = undefinedIsNull;

        if (!tuple.nullable && tuple.undefinedIsNull) {
            processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "item not nullable but can be set to undefined", element);
        }
        return tuple.nullable;
    }

    private boolean validateAccessorType(final Element element, final TypeMirror mirror) {
        final Types types = processingEnv.getTypeUtils();
        switch (mirror.getKind()) {
            case BOOLEAN:
            case INT:
            case BYTE:
            case CHAR:
            case SHORT:
            case LONG:
            case FLOAT:
            case DOUBLE:
                return true;
            case DECLARED:
                // Check if it is an autoboxed type
                if (types.isSameType(mirror, sBooleanBox)
                        || types.isSameType(mirror, sCharBox)
                        || types.isSameType(mirror, sByteBox)
                        || types.isSameType(mirror, sShortBox)
                        || types.isSameType(mirror, sIntBox)
                        || types.isSameType(mirror, sLongBox)
                        || types.isSameType(mirror, sFloatBox)
                        || types.isSameType(mirror, sDoubleBox)) {
                    return true;
                }
                // String, Object and our own V8 types are also allowed
                if (types.isSameType(mirror, sObjectType)
                        || types.isSameType(mirror, sStringType)
                        || types.isSameType(mirror, sV8ArrayType)
                        || types.isSameType(mirror, sV8GenericObjectType)
                        || types.isSameType(mirror, sV8FunctionType)) {
                    return true;
                }

                // Also accept subclasses of V8Object
                if (types.isSubtype(mirror, sV8ObjectType)) {
                    return true;
                }

                // We can't accept other types
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "Cannot call accessor with type " + mirror + "  which is neither primitive, boxed or JNIV8 specific", element);
                return false;
            default:
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "Cannot call accessor with type " + mirror, element);
                return false;
        }
    }

    /*-----------------------
     * Our own helper methods
     */

    private String getJniCodeForType(final Element element, final TypeMirror mirror, final boolean voidIsAllowed) {
        switch (mirror.getKind()) {
            case BOOLEAN:
                return "Z";
            case INT:
                return "I";
            case BYTE:
                return "B";
            case CHAR:
                return "C";
            case SHORT:
                return "S";
            case LONG:
                return "J";
            case FLOAT:
                return "F";
            case DOUBLE:
                return "D";
            case DECLARED:
                final StringBuilder sb = new StringBuilder("L");
                Util.typeToString(mirror, sb, '$');
                sb.append(";");
                return sb.toString().replace('.', '/');
            default:
                if (mirror.getKind() == TypeKind.VOID && voidIsAllowed) {
                    return "V";
                }
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "Cannot parse jni code for type " + mirror, element);
                return null;
        }
    }
}