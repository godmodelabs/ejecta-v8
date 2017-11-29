package ag.boersego.v8annotations.compiler;

import com.squareup.javapoet.ClassName;
import com.squareup.javapoet.TypeName;

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
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.TypeElement;
import javax.lang.model.element.VariableElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.DeclaredType;
import javax.lang.model.type.ErrorType;
import javax.lang.model.type.ExecutableType;
import javax.lang.model.type.PrimitiveType;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.lang.model.type.TypeVariable;
import javax.lang.model.util.Elements;
import javax.lang.model.util.SimpleTypeVisitor6;
import javax.lang.model.util.Types;
import javax.tools.Diagnostic;
import javax.tools.JavaFileObject;

import ag.boersego.v8annotations.V8Class;
import ag.boersego.v8annotations.V8ClassCreationPolicy;
import ag.boersego.v8annotations.V8Function;
import ag.boersego.v8annotations.V8Getter;
import ag.boersego.v8annotations.V8Setter;

@SupportedAnnotationTypes("ag.boersego.v8annotations.V8Function")
@SupportedSourceVersion(SourceVersion.RELEASE_7)
public final class V8AnnotationProcessor extends AbstractProcessor {

    private static TypeMirror sBooleanBox, sCharBox, sLongBox, sIntBox, sFloatBox, sDoubleBox, sShortBox, sByteBox;
    private static TypeMirror sObjectType, sStringType;
    private static TypeMirror sV8FunctionType, sV8GenericObjectType, sV8ObjectType, sV8ArrayType, sV8UndefinedType;

    private static class AccessorTuple {
        String property;
        Element getter;
        Element setter;
        TypeMirror kind;
        TypeMirror setterKind;
        public boolean nullable;
    }

    private static class AnnotationHolder {
        private TypeElement classElement;
        private ArrayList<Element> annotatedFunctions;
        private ArrayList<AccessorTuple> annotatedAccessors;
        private boolean createFromJavaOnly;
    }

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
        char c[] = string.toCharArray();
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
        for (Element e : holder.annotatedFunctions) {
            String methodName = e.getSimpleName().toString();
            String property = e.getAnnotation(V8Function.class).property();
            if (property.isEmpty()) {
                property = methodName;
            }
            boolean isStatic = e.getModifiers().contains(Modifier.STATIC);
            builder.append("\t\t\t").append(index++ == 0 ? "" : ",").append("new V8FunctionInfo(\"").append(property).append("\",\"").append(methodName).append("\",").append(isStatic ? "true" : "false").append(")\n");
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
                typeCode = getJniCodeForType(tuple.getter, tuple.kind);
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

            builder.append(isGetterStatic ? "true, " : "false, ").append(tuple.nullable ? "true" : "false").append(")\n");
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

    /** Returns a string for the raw type of {@code type}. Primitive types are always boxed. */
    public static String rawTypeToString(TypeMirror type, char innerClassSeparator) {
        if (!(type instanceof DeclaredType)) {
            throw new IllegalArgumentException("Unexpected type: " + type);
        }
        StringBuilder result = new StringBuilder();
        DeclaredType declaredType = (DeclaredType) type;
        rawTypeToString(result, (TypeElement) declaredType.asElement(), innerClassSeparator);
        return result.toString();
    }

    private static void rawTypeToString(StringBuilder result, TypeElement type,
                                        char innerClassSeparator) {
        String packageName = getPackage(type).getQualifiedName().toString();
        String qualifiedName = type.getQualifiedName().toString();
        if (packageName.isEmpty()) {
            result.append(qualifiedName.replace('.', innerClassSeparator));
        } else {
            result.append(packageName);
            result.append('.');
            result.append(
                    qualifiedName.substring(packageName.length() + 1).replace('.', innerClassSeparator));
        }
    }

    private static PackageElement getPackage(Element type) {
        while (type.getKind() != ElementKind.PACKAGE) {
            type = type.getEnclosingElement();
        }
        return (PackageElement) type;
    }

    private static TypeName box(PrimitiveType primitiveType) {
        switch (primitiveType.getKind()) {
            case BYTE:
                return ClassName.get(Byte.class);
            case SHORT:
                return ClassName.get(Short.class);
            case INT:
                return ClassName.get(Integer.class);
            case LONG:
                return ClassName.get(Long.class);
            case FLOAT:
                return ClassName.get(Float.class);
            case DOUBLE:
                return ClassName.get(Double.class);
            case BOOLEAN:
                return ClassName.get(Boolean.class);
            case CHAR:
                return ClassName.get(Character.class);
            case VOID:
                return ClassName.get(Void.class);
            default:
                throw new AssertionError();
        }
    }

    private static void typeToString(final TypeMirror type, final StringBuilder result, final char innerClassSeparator)  {
        type.accept(new SimpleTypeVisitor6<Void, Void>()
        {
            @Override
            public Void visitDeclared(DeclaredType declaredType, Void v)
            {
                TypeElement typeElement = (TypeElement) declaredType.asElement();

                rawTypeToString(result, typeElement, innerClassSeparator);

                List<? extends TypeMirror> typeArguments = declaredType.getTypeArguments();
                if (!typeArguments.isEmpty())
                {
                    result.append("<");
                    for (int i = 0; i < typeArguments.size(); i++)
                    {
                        if (i != 0)
                        {
                            result.append(", ");
                        }

                        // NOTE: Recursively resolve the types
                        typeToString(typeArguments.get(i), result, innerClassSeparator);
                    }

                    result.append(">");
                }

                return null;
            }

            @Override
            public Void visitPrimitive(PrimitiveType primitiveType, Void v) {
                result.append(box((PrimitiveType) type));
                return null;
            }

            @Override
            public Void visitArray(ArrayType arrayType, Void v) {
                TypeMirror type = arrayType.getComponentType();
                if (type instanceof PrimitiveType) {
                    result.append(type.toString()); // Don't box, since this is an array.
                } else {
                    typeToString(arrayType.getComponentType(), result, innerClassSeparator);
                }
                result.append("[]");
                return null;
            }

            @Override
            public Void visitTypeVariable(TypeVariable typeVariable, Void v)
            {
                result.append(typeVariable.asElement().getSimpleName());
                return null;
            }

            @Override
            public Void visitError(ErrorType errorType, Void v) {
                throw new UnsupportedOperationException(
                        "Unexpected TypeKind " + errorType.getKind() + " for "  + type);
            }

            @Override
            protected Void defaultAction(TypeMirror typeMirror, Void v) {
                throw new UnsupportedOperationException(
                        "Unexpected TypeKind " + typeMirror.getKind() + " for "  + typeMirror);
            }
        }, null);
    }

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
            if (!types.isSameType(emeth.getReturnType(), sObjectType)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "annotated method must have return type Object", element);
            }
            if (emeth.getParameterTypes().size() != 1) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "annotated method must have exactly one parameters", element);
            } else {
                TypeMirror param0 = emeth.getParameterTypes().get(0);
                if (!types.isSameType(param0, objectArrayType)) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "parameter of annotated method must be of type Object[]", element);
                }
            }
            // store
            AnnotationHolder holder = getHolder(annotatedClasses, element);
            holder.annotatedFunctions.add(element);
        }
        TypeMirror getterKind;
        for (Element element : env.getElementsAnnotatedWith(V8Getter.class)) {
            // determine property name
            String property = element.getAnnotation(V8Getter.class).property();
            if (property.isEmpty()) {
                String name = element.getSimpleName().toString();
                if (name.indexOf("get") != 0 && name.indexOf("is") != 0) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "no property name specified and method does not start with 'get' or 'is'", element);
                }
                property = lcfirst(name.indexOf("get") == 0 ? name.substring(3) : name.substring(2));
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
            if (tuple.setterKind != null) {
                if (!types.isSameType(getterKind, tuple.setterKind)) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "annotated method must return same type as setter " + tuple.setterKind, element);
                }
            }
            if (validateAccessorType(element, getterKind)) {
                tuple.kind = getterKind;
            }
        }
        for (Element element : env.getElementsAnnotatedWith(V8Setter.class)) {
            // determine property name
            String property = element.getAnnotation(V8Setter.class).property();
            if (property.isEmpty()) {
                String name = element.getSimpleName().toString();
                if (name.indexOf("set") != 0) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "no property name specified and method does not start with 'set'", element);
                }
                property = lcfirst(name.substring(3));
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
            // store

            if (validateAccessorType(element, emeth.getParameterTypes().get(0))) {
                tuple.setterKind = emeth.getParameterTypes().get(0);
                tuple.nullable = isAccessorNullable(element, tuple.setterKind);
            }
            tuple.setter = element;
        }
        for (String key : annotatedClasses.keySet()) {
            AnnotationHolder holder = annotatedClasses.get(key);
            generateBinding(holder);
        }

        return true;
    }

    private boolean isAccessorNullable(final Element element, final TypeMirror mirror) {
        if (mirror.getKind().isPrimitive()) {
            return false;
        }

        VariableElement param = ((ExecutableElement) element).getParameters().get(0);
        List<? extends AnnotationMirror> mirrors = param.getAnnotationMirrors();
        for (AnnotationMirror annotation : mirrors) {
            final String annotationName = annotation.toString();
            // processingEnv.getMessager().printMessage(Diagnostic.Kind.MANDATORY_WARNING, "Annotationmirror is " + annotationName, element);
            if (annotationName.endsWith(".NotNull")) {
                return false;
            }
            if (annotationName.endsWith(".Nullable")) {
                return true;
            }
        }

        return true;
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

    private String getJniCodeForType(final Element element, final TypeMirror mirror) {
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
                typeToString(mirror, sb, '$');
                sb.append(";");
                return sb.toString().replace('.', '/');
            default:
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "Cannot parse jni code for type " + mirror, element);
                return null;
        }
    }
}