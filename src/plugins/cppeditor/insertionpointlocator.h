// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.h"
#include "cpprefactoringchanges.h"

namespace CPlusPlus {
class Namespace;
class NamespaceAST;
class Symbol;
} // namespace CPlusPlus

namespace CppEditor {

class CPPEDITOR_EXPORT InsertionLocation
{
public:
    InsertionLocation();
    InsertionLocation(const QString &fileName, const QString &prefix,
                      const QString &suffix, int line, int column);

    QString fileName() const
    { return m_fileName; }

    /// \returns The prefix to insert before any other text.
    QString prefix() const
    { return m_prefix; }

    /// \returns The suffix to insert after the other inserted text.
    QString suffix() const
    { return m_suffix; }

    /// \returns The line where to insert. The line number is 1-based.
    int line() const
    { return m_line; }

    /// \returns The column where to insert. The column number is 1-based.
    int column() const
    { return m_column; }

    bool isValid() const
    { return !m_fileName.isEmpty() && m_line > 0 && m_column > 0; }

private:
    QString m_fileName;
    QString m_prefix;
    QString m_suffix;
    int m_line = 0;
    int m_column = 0;
};

class CPPEDITOR_EXPORT InsertionPointLocator
{
public:
    enum AccessSpec {
        Invalid = -1,
        Signals = 0,

        Public = 1,
        Protected = 2,
        Private = 3,

        SlotBit = 1 << 2,

        PublicSlot    = Public    | SlotBit,
        ProtectedSlot = Protected | SlotBit,
        PrivateSlot   = Private   | SlotBit
    };
    static QString accessSpecToString(InsertionPointLocator::AccessSpec xsSpec);

    enum Position {
        AccessSpecBegin,
        AccessSpecEnd,
    };

    enum class ForceAccessSpec { Yes, No };

public:
    explicit InsertionPointLocator(const CppRefactoringChanges &refactoringChanges);

    InsertionLocation methodDeclarationInClass(
            const QString &fileName,
            const CPlusPlus::Class *clazz,
            AccessSpec xsSpec,
            ForceAccessSpec forceAccessSpec = ForceAccessSpec::No
            ) const;

    InsertionLocation methodDeclarationInClass(
            const CPlusPlus::TranslationUnit *tu,
            const CPlusPlus::ClassSpecifierAST *clazz,
            AccessSpec xsSpec,
            Position positionInAccessSpec = AccessSpecEnd,
            ForceAccessSpec forceAccessSpec = ForceAccessSpec::No
            ) const;

    InsertionLocation constructorDeclarationInClass(const CPlusPlus::TranslationUnit *tu,
                                                    const CPlusPlus::ClassSpecifierAST *clazz,
                                                    AccessSpec xsSpec,
                                                    int constructorArgumentCount) const;

    const QList<InsertionLocation> methodDefinition(
            CPlusPlus::Symbol *declaration,
            bool useSymbolFinder = true,
            const QString &destinationFile = QString()) const;

private:
    CppRefactoringChanges m_refactoringChanges;
};

// TODO: We should use the "CreateMissing" approach everywhere.
enum class NamespaceHandling { CreateMissing, Ignore };
InsertionLocation CPPEDITOR_EXPORT
insertLocationForMethodDefinition(CPlusPlus::Symbol *symbol,
                                  const bool useSymbolFinder,
                                  NamespaceHandling namespaceHandling,
                                  const CppRefactoringChanges &refactoring,
                                  const QString &fileName,
                                  QStringList *insertedNamespaces = nullptr);

} // namespace CppEditor
