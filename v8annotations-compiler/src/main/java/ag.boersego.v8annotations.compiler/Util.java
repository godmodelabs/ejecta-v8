package ag.boersego.v8annotations.compiler;

import com.squareup.javapoet.ClassName;
import com.squareup.javapoet.TypeName;

import java.util.List;

import javax.lang.model.element.Element;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.TypeElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.DeclaredType;
import javax.lang.model.type.ErrorType;
import javax.lang.model.type.PrimitiveType;
import javax.lang.model.type.TypeMirror;
import javax.lang.model.type.TypeVariable;
import javax.lang.model.util.SimpleTypeVisitor6;

/**
 * Created by Kevin Read <me@kevin-read.com> on 29.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 BörseGo AG. All rights reserved.
 */

public class Util {
    /** Returns a string for the raw type of {@code type}. Primitive types are always boxed. */
    public static String rawTypeToString(TypeMirror type, char innerClassSeparator) {
        if (!(type instanceof DeclaredType declaredType)) {
            throw new IllegalArgumentException("Unexpected type: " + type);
        }
        StringBuilder result = new StringBuilder();
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

    static void typeToString(final TypeMirror type, final StringBuilder result, final char innerClassSeparator)  {
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
                    result.append(type); // Don't box, since this is an array.
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
}
