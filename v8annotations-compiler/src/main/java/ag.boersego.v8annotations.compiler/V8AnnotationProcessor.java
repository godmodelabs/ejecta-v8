package ag.boersego.v8annotations.compiler;

import ag.boersego.v8annotations.V8Function;
import ag.boersego.v8annotations.V8Getter;
import ag.boersego.v8annotations.V8Setter;

import java.io.IOException;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Set;

import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.RoundEnvironment;
import javax.annotation.processing.SupportedAnnotationTypes;
import javax.annotation.processing.SupportedSourceVersion;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.Element;
import javax.lang.model.element.TypeElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.ExecutableType;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.lang.model.util.Elements;
import javax.lang.model.util.Types;
import javax.tools.Diagnostic;
import javax.tools.JavaFileObject;

@SupportedAnnotationTypes("ag.boersego.v8annotations.V8Function")
@SupportedSourceVersion(SourceVersion.RELEASE_7)
public final class V8AnnotationProcessor extends AbstractProcessor {
    private static class AccessorTuple {
        public String property;
        public Element getter;
        public Element setter;
    };
    private static class AnnotationHolder {
        public TypeElement classElement;
        public ArrayList<Element> annotatedFunctions;
        public ArrayList<AccessorTuple> annotatedAccessors;
    };

    private AnnotationHolder getHolder(HashMap<String, AnnotationHolder> annotatedClasses, Element element) {
        TypeElement classElement = TypeElement.class.cast(element.getEnclosingElement());
        String name = classElement.getQualifiedName().toString();

        AnnotationHolder holder;
        holder = annotatedClasses.get(name);
        if(holder == null) {
            holder = new AnnotationHolder();
            holder.annotatedFunctions = new ArrayList<Element>();
            holder.annotatedAccessors = new ArrayList<AccessorTuple>();
            holder.classElement = classElement;
            annotatedClasses.put(name, holder);
        }

        return holder;
    }

    private AccessorTuple getAccessorTuple(HashMap<String, AnnotationHolder> annotatedClasses, Element element, String property) {
        AnnotationHolder holder = getHolder(annotatedClasses, element);
        AccessorTuple tuple = null;

        for(AccessorTuple t : holder.annotatedAccessors) {
            if(t.property.equals(property)) {
                tuple = t;
                break;
            }
        }
        if(tuple == null) {
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

    @Override
    public boolean process(
            Set<? extends TypeElement> annotations,
            RoundEnvironment env) {

        HashMap<String, AnnotationHolder> annotatedClasses = new HashMap<String, AnnotationHolder>();

        Elements elems = processingEnv.getElementUtils();
        Types types = processingEnv.getTypeUtils();
        TypeMirror objectType = elems.getTypeElement("java.lang.Object").asType();
        ArrayType objectArrayType = types.getArrayType(objectType);
        for (Element element : env.getElementsAnnotatedWith(V8Function.class)) {
            // validate signature
            ExecutableType emeth = (ExecutableType)element.asType();
            if(!types.isSameType(emeth.getReturnType(), objectType)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"annotated method must have return type Object", element);
            }
            if(emeth.getParameterTypes().size() != 1) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"annotated method must have exactly one parameters", element);
            } else {
                TypeMirror param0 = emeth.getParameterTypes().get(0);
                if(!types.isSameType(param0, objectArrayType)) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"parameter of annotated method must be of type Object[]", element);
                }
            }
            // store
            AnnotationHolder holder = getHolder(annotatedClasses, element);
            holder.annotatedFunctions.add(element);
        }
        for (Element element : env.getElementsAnnotatedWith(V8Getter.class)) {
            // determine property name
            String property = element.getAnnotation(V8Getter.class).property();
            if(property.isEmpty()) {
                String name = element.getSimpleName().toString();
                if(name.indexOf("get")!=0) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "no property name specified and method does not start with 'get'", element);
                }
                property = lcfirst(name.substring(3));
            }
            // validate signature
            ExecutableType emeth = (ExecutableType)element.asType();
            if(!types.isSameType(emeth.getReturnType(), objectType)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"annotated method must have return type Object", element);
            }
            if(emeth.getParameterTypes().size() != 0) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"annotated method must not require parameters", element);
            }
            // store
            AccessorTuple tuple = getAccessorTuple(annotatedClasses, element, property);
            tuple.getter = element;
        }
        for (Element element : env.getElementsAnnotatedWith(V8Setter.class)) {
            // determine property name
            String property = element.getAnnotation(V8Setter.class).property();
            if(property.isEmpty()) {
                String name = element.getSimpleName().toString();
                if(name.indexOf("set")!=0) {
                    processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, "no property name specified and method does not start with 'set'", element);
                }
                property = lcfirst(name.substring(3));
            }
            // validate signature
            ExecutableType emeth = (ExecutableType)element.asType();
            if(!emeth.getReturnType().getKind().equals(TypeKind.VOID)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"annotated method must have return type void", element);
            }
            if(emeth.getParameterTypes().size() != 1 || !types.isSameType(emeth.getParameterTypes().get(0), objectType)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,"annotated method must accept exactly one parameter of type Object", element);
            }
            // store
            AccessorTuple tuple = getAccessorTuple(annotatedClasses, element, property);
            tuple.setter = element;
        }
        for(String key : annotatedClasses.keySet()) {
            AnnotationHolder holder = annotatedClasses.get(key);
            generateBinding(holder);
        }

        return true;
    }

    private void generateBinding(AnnotationHolder holder) {
        String className = holder.classElement.getQualifiedName().toString();
        String generatedClassName = className + "V8Binding";

        String pkg = generatedClassName.substring(0, className.lastIndexOf("."));
        String name = generatedClassName.substring(className.lastIndexOf(".")+1);

        long index;

        StringBuilder builder = new StringBuilder()
                .append("package "+pkg+";\n\n")
                .append("import ag.boersego.v8annotations.generated.V8FunctionInfo;\n")
                .append("import ag.boersego.v8annotations.generated.V8AccessorInfo;\n\n")
                .append("public class " + name+ " {\n\n") // open class
                .append("\tpublic static V8FunctionInfo[] getV8Functions() {\n") // open method
                .append("\t\tV8FunctionInfo[] res = {\n");

        index = 0;
        for(Element e : holder.annotatedFunctions) {
            String methodName = e.getSimpleName().toString();
            String property = e.getAnnotation(V8Function.class).property();
            if(property.isEmpty()) {
                property = methodName;
            }
            builder.append("\t\t\t"+(index++==0?"":",")+"new V8FunctionInfo(\"" + property + "\",\"" + methodName + "\")\n");
        }

        builder.append("\t\t};\n")
                .append("\t\treturn res;\n")
                .append("\t}\n\n") // close method
                .append("\tpublic static V8AccessorInfo[] getV8Accessors() {\n") // open method
                .append("\t\tV8AccessorInfo[] res = {\n");

        index = 0;
        for(AccessorTuple tuple : holder.annotatedAccessors) {
            String getterName = tuple.getter!=null ? tuple.getter.getSimpleName().toString() : null;
            String setterName = tuple.getter!=null ? tuple.setter.getSimpleName().toString() : null;
            builder.append("\t\t\t"+(index++==0?"":",")+"new V8AccessorInfo(\""+tuple.property+"\",\""+getterName+"\",\""+setterName+"\")\n");
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
}