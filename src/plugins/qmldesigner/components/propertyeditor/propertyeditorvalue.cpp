// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "propertyeditorvalue.h"
#include "variantproperty.h"
#include "documentmanager.h"

#include <abstractview.h>
#include <bindingproperty.h>
#include <designdocument.h>
#include <nodelistproperty.h>
#include <nodemetainfo.h>
#include <nodeproperty.h>
#include <qmldesignerplugin.h>
#include <qmlobjectnode.h>
#include <designermcumanager.h>
#include <qmlitemnode.h>

#include <utils/qtcassert.h>

#include <QRegularExpression>
#include <QUrl>
#include <QScopedPointer>
#include <assetslibrarymodel.h>

//using namespace QmlDesigner;

PropertyEditorValue::PropertyEditorValue(QObject *parent)
    : QObject(parent),
      m_isInSubState(false),
      m_isInModel(false),
      m_isBound(false),
      m_isValid(false),
      m_complexNode(new PropertyEditorNodeWrapper(this))
{
}

QVariant PropertyEditorValue::value() const
{
    QVariant returnValue = m_value;
    if (auto metaInfo = modelNode().metaInfo(); metaInfo.property(name()).propertyType().isUrl()) {
        returnValue = returnValue.toUrl().toString();
    }

    return returnValue;
}

static bool cleverDoubleCompare(const QVariant &value1, const QVariant &value2)
{ //we ignore slight changes on doubles
    if ((value1.type() == QVariant::Double) && (value2.type() == QVariant::Double)) {
        if (qFuzzyCompare(value1.toDouble(), value2.toDouble()))
            return true;
    }
    return false;
}

static bool cleverColorCompare(const QVariant &value1, const QVariant &value2)
{
    if ((value1.type() == QVariant::Color) && (value2.type() == QVariant::Color)) {
        QColor c1 = value1.value<QColor>();
        QColor c2 = value2.value<QColor>();
        QString a = c1.name();
        QString b = c2.name();
        if (a != b)
            return false;
        return (c1.alpha() == c2.alpha());
    }
    if ((value1.type() == QVariant::String) && (value2.type() == QVariant::Color))
        return cleverColorCompare(QVariant(QColor(value1.toString())), value2);
    if ((value1.type() == QVariant::Color) && (value2.type() == QVariant::String))
        return cleverColorCompare(value1, QVariant(QColor(value2.toString())));
    return false;
}


/* "red" is the same color as "#ff0000"
  To simplify editing we convert all explicit color names in the hash format */
static void fixAmbigousColorNames(const QmlDesigner::ModelNode &modelNode,
                                  const QmlDesigner::PropertyName &name,
                                  QVariant *value)
{
    if (auto metaInfo = modelNode.metaInfo(); metaInfo.property(name).propertyType().isColor()) {
        if ((value->type() == QVariant::Color)) {
            QColor color = value->value<QColor>();
            int alpha = color.alpha();
            color = QColor(color.name());
            color.setAlpha(alpha);
            *value = color;
        } else if (value->toString() != QStringLiteral("transparent")) {
            *value = QColor(value->toString()).name(QColor::HexArgb);
        }
    }
}

static void fixUrl(const QmlDesigner::ModelNode &modelNode, const QmlDesigner::PropertyName &name, QVariant *value)
{
    if (auto metaInfo = modelNode.metaInfo(); metaInfo.property(name).propertyType().isUrl()) {
        if (!value->isValid())
            *value = QStringLiteral("");
    }
}

static bool compareVariants(const QVariant &value1, const QVariant &value2)
/* The comparison of variants is not symmetric because of implicit conversion.
 * QVariant(string) == QVariant(QColor) does for example ignore the alpha channel,
 * because the color is converted to a string ignoring the alpha channel.
 * By comparing the variants in both directions we gain a symmetric comparison.
 */
{
    return (value1 == value2)
            && (value2 == value1);
}

void PropertyEditorValue::setValueWithEmit(const QVariant &value)
{
    if (!compareVariants(value, m_value ) || isBound()) {
        QVariant newValue = value;
        if (auto metaInfo = modelNode().metaInfo(); metaInfo.property(name()).propertyType().isUrl()) {
            newValue = QUrl(newValue.toString());
        }

        if (cleverDoubleCompare(newValue, m_value))
            return;
        if (cleverColorCompare(newValue, m_value))
            return;

        setValue(newValue);
        m_isBound = false;
        m_expression.clear();
        emit valueChanged(nameAsQString(), value);
        emit valueChangedQml();
        emit isBoundChanged();
        emit isExplicitChanged();
    }
}

void PropertyEditorValue::setValue(const QVariant &value)
{
    const bool colorsEqual = cleverColorCompare(value, m_value);

    if (!compareVariants(m_value, value) &&
            !cleverDoubleCompare(value, m_value) &&
            !colorsEqual)
        m_value = value;

    fixAmbigousColorNames(modelNode(), name(), &m_value);
    fixUrl(modelNode(), name(), &m_value);

    if (!colorsEqual)
        emit valueChangedQml();

    emit isExplicitChanged();
    emit isBoundChanged();
}

QString PropertyEditorValue::enumeration() const
{
    return m_value.value<QmlDesigner::Enumeration>().nameToString();
}

QString PropertyEditorValue::expression() const
{
    return m_expression;
}

void PropertyEditorValue::setExpressionWithEmit(const QString &expression)
{
    if ( m_expression != expression) {
        setExpression(expression);
        m_value.clear();
        emit expressionChanged(nameAsQString()); //Note that we set the name in this case
    }
}

void PropertyEditorValue::setExpression(const QString &expression)
{
    if ( m_expression != expression) {
        m_expression = expression;
        emit expressionChanged(QString());
    }
}

QString PropertyEditorValue::valueToString() const
{
    return value().toString();
}

bool PropertyEditorValue::isInSubState() const
{
    const QmlDesigner::QmlObjectNode objectNode(modelNode());
    return objectNode.isValid() && objectNode.currentState().isValid() && objectNode.propertyAffectedByCurrentState(name());
}

bool PropertyEditorValue::isBound() const
{
    const QmlDesigner::QmlObjectNode objectNode(modelNode());
    return objectNode.isValid() && objectNode.hasBindingProperty(name());
}

bool PropertyEditorValue::isInModel() const
{
    return modelNode().hasProperty(name());
}

QmlDesigner::PropertyName PropertyEditorValue::name() const
{
    return m_name;
}

QString PropertyEditorValue::nameAsQString() const
{
    return QString::fromUtf8(m_name);
}

void PropertyEditorValue::setName(const QmlDesigner::PropertyName &name)
{
    m_name = name;
}


bool PropertyEditorValue::isValid() const
{
    return m_isValid;
}

void PropertyEditorValue::setIsValid(bool valid)
{
    m_isValid = valid;
}

bool PropertyEditorValue::isTranslated() const
{
    if (modelNode().isValid()) {
        if (auto metaInfo = modelNode().metaInfo();
            metaInfo.isValid() && metaInfo.hasProperty(name())
            && metaInfo.property(name()).propertyType().isString()) {
            const QmlDesigner::QmlObjectNode objectNode(modelNode());
            if (objectNode.hasBindingProperty(name())) {
                const QRegularExpression rx(
                    QRegularExpression::anchoredPattern("qsTr(|Id|anslate)\\(\".*\"\\)"));
                //qsTr()
                if (objectNode.propertyAffectedByCurrentState(name())) {
                    return expression().contains(rx);
                } else {
                    return modelNode().bindingProperty(name()).expression().contains(rx);
                }
            }
        }
    }
    return false;
}

bool PropertyEditorValue::hasActiveDrag() const
{
    return m_hasActiveDrag;
}

void PropertyEditorValue::setHasActiveDrag(bool val)
{
    if (m_hasActiveDrag != val) {
        m_hasActiveDrag = val;
        emit hasActiveDragChanged();
    }
}

static bool isAllowedSubclassType(const QString &type,
                                  const QmlDesigner::NodeMetaInfo &metaInfo,
                                  QmlDesigner::Model *model)
{
    if (!metaInfo.isValid())
        return false;

    auto base = model->metaInfo(type.toUtf8());

    return metaInfo.isBasedOn(base);
}

bool PropertyEditorValue::isAvailable() const
{
    if (!m_modelNode.isValid())
        return true;

    const QmlDesigner::DesignerMcuManager &mcuManager = QmlDesigner::DesignerMcuManager::instance();

    if (mcuManager.isMCUProject()) {
        const QSet<QString> nonMcuProperties = mcuManager.bannedProperties();
        const auto mcuAllowedItemProperties = mcuManager.allowedItemProperties();
        const auto mcuBannedComplexProperties = mcuManager.bannedComplexProperties();

        const QList<QByteArray> list = name().split('.');
        const QByteArray pureName = list.constFirst();
        const QString pureNameStr = QString::fromUtf8(pureName);

        const QByteArray ending = list.constLast();
        const QString endingStr = QString::fromUtf8(ending);

        //allowed item properties:
        const auto itemTypes = mcuAllowedItemProperties.keys();
        for (const auto &itemType : itemTypes) {
            if (isAllowedSubclassType(itemType, m_modelNode.metaInfo(), m_modelNode.model())) {
                const QmlDesigner::DesignerMcuManager::ItemProperties allowedItemProps =
                        mcuAllowedItemProperties.value(itemType);
                if (allowedItemProps.properties.contains(pureNameStr)) {
                    if (QmlDesigner::QmlItemNode::isValidQmlItemNode(m_modelNode)) {
                        const bool itemHasChildren = QmlDesigner::QmlItemNode(m_modelNode).hasChildren();

                        if (itemHasChildren)
                            return allowedItemProps.allowChildren;

                        return true;
                    }
                }
            }
        }

        //banned properties:
        //with prefixes:
        if (mcuBannedComplexProperties.value(pureNameStr).contains(endingStr))
            return false;

        //general group:
        if (nonMcuProperties.contains(pureNameStr))
            return false;

    }

    return true;
}

QmlDesigner::ModelNode PropertyEditorValue::modelNode() const
{
    return m_modelNode;
}

void PropertyEditorValue::setModelNode(const QmlDesigner::ModelNode &modelNode)
{
    if (modelNode != m_modelNode) {
        m_modelNode = modelNode;
        m_complexNode->update();
        emit modelNodeChanged();
    }
}

PropertyEditorNodeWrapper* PropertyEditorValue::complexNode()
{
    return m_complexNode;
}

void PropertyEditorValue::resetValue()
{
    if (m_value.isValid() || isBound()) {
        m_value = QVariant();
        m_isBound = false;
        m_expression = QString();
        emit valueChanged(nameAsQString(), QVariant());
        emit expressionChanged({});
    }
}

void PropertyEditorValue::setEnumeration(const QString &scope, const QString &name)
{
    QmlDesigner::Enumeration newEnumeration(scope, name);

    setValueWithEmit(QVariant::fromValue(newEnumeration));
}

void PropertyEditorValue::exportPropertyAsAlias()
{
    emit exportPropertyAsAliasRequested(nameAsQString());
}

bool PropertyEditorValue::hasPropertyAlias() const
{
    if (!modelNode().isValid())
        return false;

    if (modelNode().isRootNode())
        return false;

    if (!modelNode().hasId())
        return false;

    QString id = modelNode().id();

    for (const QmlDesigner::BindingProperty &property : modelNode().view()->rootModelNode().bindingProperties())
        if (property.expression() == (id + "." + nameAsQString()))
            return true;

    return false;
}

bool PropertyEditorValue::isAttachedProperty() const
{
    if (nameAsQString().isEmpty())
        return false;

    return nameAsQString().at(0).isUpper();
}

void PropertyEditorValue::removeAliasExport()
{
    emit removeAliasExportRequested(nameAsQString());
}

QString PropertyEditorValue::getTranslationContext() const
{
    if (modelNode().isValid()) {
        if (auto metaInfo = modelNode().metaInfo();
            metaInfo.isValid() && metaInfo.hasProperty(name())
            && metaInfo.property(name()).propertyType().isString()) {
            const QmlDesigner::QmlObjectNode objectNode(modelNode());
            if (objectNode.hasBindingProperty(name())) {
                const QRegularExpression rx(QRegularExpression::anchoredPattern(
                    "qsTranslate\\(\"(.*)\"\\s*,\\s*\".*\"\\s*\\)"));
                const QRegularExpressionMatch match = rx.match(expression());
                if (match.hasMatch())
                    return match.captured(1);
            }
        }
    }
    return QString();
}

bool PropertyEditorValue::isIdList() const
{
    if (modelNode().isValid() && modelNode().metaInfo().isValid() && modelNode().metaInfo().hasProperty(name())) {
        const QmlDesigner::QmlObjectNode objectNode(modelNode());
        if (objectNode.hasBindingProperty(name())) {
            static const QRegularExpression rx(QRegularExpression::anchoredPattern(
                                                   "^[a-z_]\\w*|^[A-Z]\\w*\\.{1}([a-z_]\\w*\\.?)+"));
            const QString exp = objectNode.propertyAffectedByCurrentState(name()) ? expression() : modelNode().bindingProperty(name()).expression();
            for (const auto &str : generateStringList(exp))
            {
                if (!str.contains(rx))
                    return false;
            }
            return true;
        }
        return false;
    }
    return false;
}

QStringList PropertyEditorValue::getExpressionAsList() const
{
    return generateStringList(expression());
}

bool PropertyEditorValue::idListAdd(const QString &value)
{
    const QmlDesigner::QmlObjectNode objectNode(modelNode());
    if (!isIdList() && (objectNode.isValid() && objectNode.hasProperty(name())))
        return false;

    static const QRegularExpression rx(QRegularExpression::anchoredPattern(
                                           "^[a-z_]\\w*|^[A-Z]\\w*\\.{1}([a-z_]\\w*\\.?)+"));
    if (!value.contains(rx))
        return false;

    auto stringList = generateStringList(expression());
    stringList.append(value);
    setExpressionWithEmit(generateString(stringList));

    return true;
}

bool PropertyEditorValue::idListRemove(int idx)
{
    QTC_ASSERT(isIdList(), return false);

    auto stringList = generateStringList(expression());
    if (idx < 0 || idx >= stringList.size())
        return false;

    stringList.removeAt(idx);
    setExpressionWithEmit(generateString(stringList));

    return true;
}

bool PropertyEditorValue::idListReplace(int idx, const QString &value)
{
    QTC_ASSERT(isIdList(), return false);

    static const QRegularExpression rx(QRegularExpression::anchoredPattern(
                                           "^[a-z_]\\w*|^[A-Z]\\w*\\.{1}([a-z_]\\w*\\.?)+"));
    if (!value.contains(rx))
        return false;

    auto stringList = generateStringList(expression());

    if (idx < 0 || idx >= stringList.size())
        return false;

    stringList.replace(idx, value);
    setExpressionWithEmit(generateString(stringList));

    return true;
}

void PropertyEditorValue::commitDrop(const QString &path)
{
    if (m_modelNode.metaInfo().isQtQuick3DMaterial()
        && m_modelNode.metaInfo().property(m_name).propertyType().isQtQuick3DTexture()) {
        // create a texture node
        QmlDesigner::NodeMetaInfo metaInfo = m_modelNode.view()->model()->metaInfo("QtQuick3D.Texture");
        QmlDesigner::ModelNode texture = m_modelNode.view()->createModelNode("QtQuick3D.Texture",
                                                                             metaInfo.majorVersion(),
                                                                             metaInfo.minorVersion());
        texture.validId();
        m_modelNode.view()->rootModelNode().defaultNodeListProperty().reparentHere(texture);
        // TODO: group textures under 1 node (just like materials)

        // set texture source
        Utils::FilePath imagePath = Utils::FilePath::fromString(path);
        Utils::FilePath currFilePath = QmlDesigner::DocumentManager::currentFilePath();
        QmlDesigner::VariantProperty srcProp = texture.variantProperty("source");
        srcProp.setValue(imagePath.relativePathFrom(currFilePath).toString());

        // assign the texture to the property
        setExpressionWithEmit(texture.id());
    }

    m_modelNode.view()->model()->endDrag();
}

QStringList PropertyEditorValue::generateStringList(const QString &string) const
{
    QString copy = string;
    copy = copy.remove("[").remove("]");

    QStringList tmp = copy.split(',', Qt::SkipEmptyParts);
    for (QString &str : tmp)
        str = str.trimmed();

    return tmp;
}

QString PropertyEditorValue::generateString(const QStringList &stringList) const
{
    if (stringList.size() > 1)
        return "[" + stringList.join(",") + "]";
    else if (stringList.isEmpty())
        return QString();
    else
        return stringList.first();
}

void PropertyEditorValue::registerDeclarativeTypes()
{
    qmlRegisterType<PropertyEditorValue>("HelperWidgets",2,0,"PropertyEditorValue");
    qmlRegisterType<PropertyEditorNodeWrapper>("HelperWidgets",2,0,"PropertyEditorNodeWrapper");
    qmlRegisterType<QQmlPropertyMap>("HelperWidgets",2,0,"QQmlPropertyMap");
}

PropertyEditorNodeWrapper::PropertyEditorNodeWrapper(PropertyEditorValue* parent) : QObject(parent), m_valuesPropertyMap(this)
{
    m_editorValue = parent;
    connect(m_editorValue, &PropertyEditorValue::modelNodeChanged, this, &PropertyEditorNodeWrapper::update);
}

PropertyEditorNodeWrapper::PropertyEditorNodeWrapper(QObject *parent) : QObject(parent), m_editorValue(nullptr)
{
}

bool PropertyEditorNodeWrapper::exists()
{
    if (!(m_editorValue && m_editorValue->modelNode().isValid()))
        return false;

    return m_modelNode.isValid();
}

QString PropertyEditorNodeWrapper::type()
{
    return m_modelNode.simplifiedTypeName();
}

QmlDesigner::ModelNode PropertyEditorNodeWrapper::parentModelNode() const
{
    return  m_editorValue->modelNode();
}

QmlDesigner::PropertyName PropertyEditorNodeWrapper::propertyName() const
{
    return m_editorValue->name();
}

QQmlPropertyMap *PropertyEditorNodeWrapper::properties()
{
    return &m_valuesPropertyMap;
}

void PropertyEditorNodeWrapper::add(const QString &type)
{
    QmlDesigner::TypeName propertyType = type.toUtf8();

    if ((m_editorValue && m_editorValue->modelNode().isValid())) {
        if (propertyType.isEmpty()) {
            propertyType = m_editorValue->modelNode()
                               .metaInfo()
                               .property(m_editorValue->name())
                               .propertyType()
                               .typeName();
        }
        while (propertyType.contains('*')) //strip star
            propertyType.chop(1);
        m_modelNode = m_editorValue->modelNode().view()->createModelNode(propertyType, 4, 7);
        m_editorValue->modelNode().nodeAbstractProperty(m_editorValue->name()).reparentHere(m_modelNode);
        if (!m_modelNode.isValid())
            qWarning("PropertyEditorNodeWrapper::add failed");
    } else {
        qWarning("PropertyEditorNodeWrapper::add failed - node invalid");
    }
    setup();
}

void PropertyEditorNodeWrapper::remove()
{
    if ((m_editorValue && m_editorValue->modelNode().isValid())) {
        QmlDesigner::QmlObjectNode(m_modelNode).destroy();
        m_editorValue->modelNode().removeProperty(m_editorValue->name());
    } else {
        qWarning("PropertyEditorNodeWrapper::remove failed - node invalid");
    }
    m_modelNode = QmlDesigner::ModelNode();

    const QStringList propertyNames = m_valuesPropertyMap.keys();
    for (const QString &propertyName : propertyNames)
        m_valuesPropertyMap.clear(propertyName);
    qDeleteAll(m_valuesPropertyMap.children());
    emit propertiesChanged();
    emit existsChanged();
}

void PropertyEditorNodeWrapper::changeValue(const QString &propertyName)
{
    const QmlDesigner::PropertyName name = propertyName.toUtf8();

    if (name.isNull())
        return;
    if (m_modelNode.isValid()) {
        QScopedPointer<QmlDesigner::QmlObjectNode> qmlObjectNode{
                QmlDesigner::QmlObjectNode::getQmlObjectNodeOfCorrectType(m_modelNode)};

        auto valueObject = qvariant_cast<PropertyEditorValue *>(m_valuesPropertyMap.value(QString::fromLatin1(name)));

        if (valueObject->value().isValid())
            qmlObjectNode->setVariantProperty(name, valueObject->value());
        else
            qmlObjectNode->removeProperty(name);
    }
}

void PropertyEditorNodeWrapper::setup()
{
    Q_ASSERT(m_editorValue);
    Q_ASSERT(m_editorValue->modelNode().isValid());
    if ((m_editorValue->modelNode().isValid() && m_modelNode.isValid())) {
        const QStringList propertyNames = m_valuesPropertyMap.keys();
        for (const QString &propertyName : propertyNames)
            m_valuesPropertyMap.clear(propertyName);
        qDeleteAll(m_valuesPropertyMap.children());

        if (QmlDesigner::QmlObjectNode qmlObjectNode = m_modelNode) {
            for (const auto &property : m_modelNode.metaInfo().properties()) {
                const auto &propertyName = property.name();
                auto valueObject = new PropertyEditorValue(&m_valuesPropertyMap);
                valueObject->setName(propertyName);
                valueObject->setValue(qmlObjectNode.instanceValue(propertyName));
                connect(valueObject, &PropertyEditorValue::valueChanged, &m_valuesPropertyMap, &QQmlPropertyMap::valueChanged);
                m_valuesPropertyMap.insert(QString::fromUtf8(propertyName), QVariant::fromValue(valueObject));
            }
        }
    }
    connect(&m_valuesPropertyMap, &QQmlPropertyMap::valueChanged, this, &PropertyEditorNodeWrapper::changeValue);

    emit propertiesChanged();
    emit existsChanged();
}

void PropertyEditorNodeWrapper::update()
{
    if (m_editorValue && m_editorValue->modelNode().isValid()) {
        if (m_editorValue->modelNode().hasProperty(m_editorValue->name()) && m_editorValue->modelNode().property(m_editorValue->name()).isNodeProperty())
            m_modelNode = m_editorValue->modelNode().nodeProperty(m_editorValue->name()).modelNode();
        setup();
        emit existsChanged();
        emit typeChanged();
    }
}
