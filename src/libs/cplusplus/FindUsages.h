// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "LookupContext.h"
#include "CppDocument.h"
#include "TypeOfExpression.h"

#include <cplusplus/ASTVisitor.h>
#include <utils/fileutils.h>

#include <QSet>

namespace CPlusPlus {

class CPLUSPLUS_EXPORT Usage
{
public:
    enum class Tag {
        Declaration = 1 << 0,
        Read = 1 << 1,
        Write = 1 << 2,
        WritableRef = 1 << 3,
        Override = 1 << 4,
        MocInvokable = 1 << 5,
        Template = 1 << 6,
    };
    using Tags = QFlags<Tag>;

    Usage() = default;
    Usage(const Utils::FilePath &path, const QString &lineText, const QString &func, Tags t,
          int line, int col, int len)
        : path(path), lineText(lineText), containingFunction(func), tags(t),
          line(line), col(col), len(len) {}

    Utils::FilePath path;
    QString lineText;
    QString containingFunction;
    Tags tags;
    int line = 0;
    int col = 0;
    int len = 0;
};

class CPLUSPLUS_EXPORT FindUsages: protected ASTVisitor
{
public:
    FindUsages(const QByteArray &originalSource, Document::Ptr doc, const Snapshot &snapshot,
               bool categorize);
    FindUsages(const LookupContext &context);

    void operator()(Symbol *symbol);

    QList<Usage> usages() const;
    QList<int> references() const;

protected:
    using ASTVisitor::translationUnit;

    Scope *switchScope(Scope *scope);

    QString matchingLine(const Token &tk) const;

    void reportResult(unsigned tokenIndex, const Name *name, Scope *scope = nullptr);
    void reportResult(unsigned tokenIndex, const QList<LookupItem> &candidates);
    Usage::Tags getTags(int line, int column, int tokenIndex);
    QString getContainingFunction(int line, int column);

    bool checkCandidates(const QList<LookupItem> &candidates) const;
    void checkExpression(unsigned startToken, unsigned endToken, Scope *scope = nullptr);

    static bool isLocalScope(Scope *scope);

    void statement(StatementAST *ast);
    void expression(ExpressionAST *ast);
    void declaration(DeclarationAST *ast);
    const Name *name(NameAST *ast);
    void specifier(SpecifierAST *ast);
    void ptrOperator(PtrOperatorAST *ast);
    void coreDeclarator(CoreDeclaratorAST *ast);
    void postfixDeclarator(PostfixDeclaratorAST *ast);

    void objCSelectorArgument(ObjCSelectorArgumentAST *ast);
    void attribute(GnuAttributeAST *ast);
    void declarator(DeclaratorAST *ast, Scope *symbol = nullptr);
    void qtPropertyDeclarationItem(QtPropertyDeclarationItemAST *ast);
    void qtInterfaceName(QtInterfaceNameAST *ast);
    void baseSpecifier(BaseSpecifierAST *ast);
    void ctorInitializer(CtorInitializerAST *ast);
    void enumerator(EnumeratorAST *ast);
    void exceptionSpecification(ExceptionSpecificationAST *ast);
    void memInitializer(MemInitializerAST *ast);
    void nestedNameSpecifier(NestedNameSpecifierAST *ast);
    void newPlacement(ExpressionListParenAST *ast);
    void newArrayDeclarator(NewArrayDeclaratorAST *ast);
    void newTypeId(NewTypeIdAST *ast);
    void cppOperator(OperatorAST *ast);
    void parameterDeclarationClause(ParameterDeclarationClauseAST *ast);
    void translationUnit(TranslationUnitAST *ast);
    void objCProtocolRefs(ObjCProtocolRefsAST *ast);
    void objCMessageArgument(ObjCMessageArgumentAST *ast);
    void objCTypeName(ObjCTypeNameAST *ast);
    void objCInstanceVariablesDeclaration(ObjCInstanceVariablesDeclarationAST *ast);
    void objCPropertyAttribute(ObjCPropertyAttributeAST *ast);
    void objCMessageArgumentDeclaration(ObjCMessageArgumentDeclarationAST *ast);
    void objCMethodPrototype(ObjCMethodPrototypeAST *ast);
    void objCSynthesizedProperty(ObjCSynthesizedPropertyAST *ast);
    void lambdaIntroducer(LambdaIntroducerAST *ast);
    void lambdaCapture(LambdaCaptureAST *ast);
    void capture(CaptureAST *ast);
    void lambdaDeclarator(LambdaDeclaratorAST *ast);
    void trailingReturnType(TrailingReturnTypeAST *ast);

    // AST
    virtual bool visit(ObjCSelectorArgumentAST *ast);
    virtual bool visit(GnuAttributeAST *ast);
    virtual bool visit(DeclaratorAST *ast);
    virtual bool visit(QtPropertyDeclarationItemAST *ast);
    virtual bool visit(QtInterfaceNameAST *ast);
    virtual bool visit(BaseSpecifierAST *ast);
    virtual bool visit(CtorInitializerAST *ast);
    virtual bool visit(EnumeratorAST *ast);
    virtual bool visit(DynamicExceptionSpecificationAST *ast);
    virtual bool visit(MemInitializerAST *ast);
    virtual bool visit(NestedNameSpecifierAST *ast);
    virtual bool visit(NewArrayDeclaratorAST *ast);
    virtual bool visit(NewTypeIdAST *ast);
    virtual bool visit(OperatorAST *ast);
    virtual bool visit(ParameterDeclarationClauseAST *ast);
    virtual bool visit(TranslationUnitAST *ast);
    virtual bool visit(ObjCProtocolRefsAST *ast);
    virtual bool visit(ObjCMessageArgumentAST *ast);
    virtual bool visit(ObjCTypeNameAST *ast);
    virtual bool visit(ObjCInstanceVariablesDeclarationAST *ast);
    virtual bool visit(ObjCPropertyAttributeAST *ast);
    virtual bool visit(ObjCMessageArgumentDeclarationAST *ast);
    virtual bool visit(ObjCMethodPrototypeAST *ast);
    virtual bool visit(ObjCSynthesizedPropertyAST *ast);
    virtual bool visit(LambdaIntroducerAST *ast);
    virtual bool visit(LambdaCaptureAST *ast);
    virtual bool visit(CaptureAST *ast);
    virtual bool visit(LambdaDeclaratorAST *ast);
    virtual bool visit(TrailingReturnTypeAST *ast);

    // StatementAST
    virtual bool visit(QtMemberDeclarationAST *ast);
    virtual bool visit(CaseStatementAST *ast);
    virtual bool visit(CompoundStatementAST *ast);
    virtual bool visit(DeclarationStatementAST *ast);
    virtual bool visit(DoStatementAST *ast);
    virtual bool visit(ExpressionOrDeclarationStatementAST *ast);
    virtual bool visit(ExpressionStatementAST *ast);
    virtual bool visit(ForeachStatementAST *ast);
    virtual bool visit(RangeBasedForStatementAST *ast);
    virtual bool visit(ForStatementAST *ast);
    virtual bool visit(IfStatementAST *ast);
    virtual bool visit(LabeledStatementAST *ast);
    virtual bool visit(BreakStatementAST *ast);
    virtual bool visit(ContinueStatementAST *ast);
    virtual bool visit(GotoStatementAST *ast);
    virtual bool visit(ReturnStatementAST *ast);
    virtual bool visit(SwitchStatementAST *ast);
    virtual bool visit(TryBlockStatementAST *ast);
    virtual bool visit(CatchClauseAST *ast);
    virtual bool visit(WhileStatementAST *ast);
    virtual bool visit(ObjCFastEnumerationAST *ast);
    virtual bool visit(ObjCSynchronizedStatementAST *ast);

    // ExpressionAST
    virtual bool visit(IdExpressionAST *ast);
    virtual bool visit(CompoundExpressionAST *ast);
    virtual bool visit(CompoundLiteralAST *ast);
    virtual bool visit(QtMethodAST *ast);
    virtual bool visit(BinaryExpressionAST *ast);
    virtual bool visit(CastExpressionAST *ast);
    virtual bool visit(ConditionAST *ast);
    virtual bool visit(ConditionalExpressionAST *ast);
    virtual bool visit(CppCastExpressionAST *ast);
    virtual bool visit(DeleteExpressionAST *ast);
    virtual bool visit(ArrayInitializerAST *ast);
    virtual bool visit(NewExpressionAST *ast);
    virtual bool visit(TypeidExpressionAST *ast);
    virtual bool visit(TypenameCallExpressionAST *ast);
    virtual bool visit(TypeConstructorCallAST *ast);
    virtual bool visit(SizeofExpressionAST *ast);
    virtual bool visit(PointerLiteralAST *ast);
    virtual bool visit(NumericLiteralAST *ast);
    virtual bool visit(BoolLiteralAST *ast);
    virtual bool visit(ThisExpressionAST *ast);
    virtual bool visit(NestedExpressionAST *ast);
    virtual bool visit(StringLiteralAST *ast);
    virtual bool visit(ThrowExpressionAST *ast);
    virtual bool visit(NoExceptOperatorExpressionAST *ast);
    virtual bool visit(TypeIdAST *ast);
    virtual bool visit(UnaryExpressionAST *ast);
    virtual bool visit(ObjCMessageExpressionAST *ast);
    virtual bool visit(ObjCProtocolExpressionAST *ast);
    virtual bool visit(ObjCEncodeExpressionAST *ast);
    virtual bool visit(ObjCSelectorExpressionAST *ast);
    virtual bool visit(LambdaExpressionAST *ast);
    virtual bool visit(BracedInitializerAST *ast);
    virtual bool visit(ExpressionListParenAST *ast);

    // DeclarationAST
    virtual bool visit(SimpleDeclarationAST *ast);
    virtual bool visit(EmptyDeclarationAST *ast);
    virtual bool visit(AccessDeclarationAST *ast);
    virtual bool visit(QtObjectTagAST *ast);
    virtual bool visit(QtPrivateSlotAST *ast);
    virtual bool visit(QtPropertyDeclarationAST *ast);
    virtual bool visit(QtEnumDeclarationAST *ast);
    virtual bool visit(QtFlagsDeclarationAST *ast);
    virtual bool visit(QtInterfacesDeclarationAST *ast);
    virtual bool visit(AsmDefinitionAST *ast);
    virtual bool visit(ExceptionDeclarationAST *ast);
    virtual bool visit(FunctionDefinitionAST *ast);
    virtual bool visit(LinkageBodyAST *ast);
    virtual bool visit(LinkageSpecificationAST *ast);
    virtual bool visit(NamespaceAST *ast);
    virtual bool visit(NamespaceAliasDefinitionAST *ast);
    virtual bool visit(ParameterDeclarationAST *ast);
    virtual bool visit(StaticAssertDeclarationAST *ast);
    virtual bool visit(TemplateDeclarationAST *ast);
    virtual bool visit(TypenameTypeParameterAST *ast);
    virtual bool visit(TemplateTypeParameterAST *ast);
    virtual bool visit(UsingAST *ast);
    virtual bool visit(UsingDirectiveAST *ast);
    virtual bool visit(ObjCClassForwardDeclarationAST *ast);
    virtual bool visit(ObjCClassDeclarationAST *ast);
    virtual bool visit(ObjCProtocolForwardDeclarationAST *ast);
    virtual bool visit(ObjCProtocolDeclarationAST *ast);
    virtual bool visit(ObjCVisibilityDeclarationAST *ast);
    virtual bool visit(ObjCPropertyDeclarationAST *ast);
    virtual bool visit(ObjCMethodDeclarationAST *ast);
    virtual bool visit(ObjCSynthesizedPropertiesDeclarationAST *ast);
    virtual bool visit(ObjCDynamicPropertiesDeclarationAST *ast);

    // NameAST
    virtual bool visit(ObjCSelectorAST *ast);
    virtual bool visit(QualifiedNameAST *ast);
    virtual bool visit(OperatorFunctionIdAST *ast);
    virtual bool visit(ConversionFunctionIdAST *ast);
    virtual bool visit(SimpleNameAST *ast);
    virtual bool visit(TemplateIdAST *ast);

    // SpecifierAST
    virtual bool visit(SimpleSpecifierAST *ast);
    virtual bool visit(GnuAttributeSpecifierAST *ast);
    virtual bool visit(MsvcDeclspecSpecifierAST *ast);
    virtual bool visit(StdAttributeSpecifierAST *ast);
    virtual bool visit(TypeofSpecifierAST *ast);
    virtual bool visit(DecltypeSpecifierAST *ast);
    virtual bool visit(ClassSpecifierAST *ast);
    virtual bool visit(NamedTypeSpecifierAST *ast);
    virtual bool visit(ElaboratedTypeSpecifierAST *ast);
    virtual bool visit(EnumSpecifierAST *ast);

    // PtrOperatorAST
    virtual bool visit(PointerToMemberAST *ast);
    virtual bool visit(PointerAST *ast);
    virtual bool visit(ReferenceAST *ast);

    // PostfixAST
    virtual bool visit(CallAST *ast);
    virtual bool visit(ArrayAccessAST *ast);
    virtual bool visit(PostIncrDecrAST *ast);
    virtual bool visit(MemberAccessAST *ast);

    // CoreDeclaratorAST
    virtual bool visit(DeclaratorIdAST *ast);
    virtual bool visit(DecompositionDeclaratorAST *ast);
    virtual bool visit(NestedDeclaratorAST *ast);

    // PostfixDeclaratorAST
    virtual bool visit(FunctionDeclaratorAST *ast);
    virtual bool visit(ArrayDeclaratorAST *ast);

private:
    void prepareLines(const QByteArray &bytes);
    QString fetchLine(unsigned lineNr) const;

private:
    const Identifier *_id = nullptr;
    Symbol *_declSymbol = nullptr;
    QList<const Name *> _declSymbolFullyQualifiedName;
    Document::Ptr _doc;
    Snapshot _snapshot;
    LookupContext _context;
    const QByteArray _originalSource;
    std::vector<const char *> _sourceLineEnds;
    QByteArray _source;
    QList<int> _references;
    QList<Usage> _usages;
    QSet<unsigned> _processed;
    TypeOfExpression typeofExpression;
    Scope *_currentScope = nullptr;
    const bool _categorize = false;
    class GetUsageTags;
};

} // namespace CPlusPlus
