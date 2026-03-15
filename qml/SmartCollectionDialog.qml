import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "IconResolver.js" as IconResolver

Dialog {
    id: dialogRoot

    property string errorText: ""
    readonly property bool compactLayout: width < 700

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    readonly property var fieldOptions: [
        { value: "text", label: tr("collections.fieldAllText"), kind: "text" },
        { value: "title", label: tr("collections.fieldTitle"), kind: "text" },
        { value: "artist", label: tr("collections.fieldArtist"), kind: "text" },
        { value: "album", label: tr("collections.fieldAlbum"), kind: "text" },
        { value: "path", label: tr("collections.fieldPath"), kind: "text" },
        { value: "format", label: tr("collections.fieldFormat"), kind: "text" },
        { value: "added_days_ago", label: tr("collections.fieldAddedDays"), kind: "number" },
        { value: "play_count", label: tr("collections.fieldPlayCount"), kind: "number" },
        { value: "skip_count", label: tr("collections.fieldSkipCount"), kind: "number" },
        { value: "rating", label: tr("collections.fieldRating"), kind: "number" },
        { value: "sample_rate", label: tr("collections.fieldSampleRate"), kind: "number" },
        { value: "bit_depth", label: tr("collections.fieldBitDepth"), kind: "number" },
        { value: "last_played_days_ago", label: tr("collections.fieldLastPlayedDays"), kind: "number" },
        { value: "favorite", label: tr("collections.fieldFavorite"), kind: "bool" }
    ]

    readonly property var textOperatorOptions: [
        { value: "match", label: tr("collections.opMatch") },
        { value: "contains", label: tr("collections.opContains") },
        { value: "starts_with", label: tr("collections.opStartsWith") },
        { value: "=", label: tr("collections.opEq") },
        { value: "!=", label: tr("collections.opNe") }
    ]

    readonly property var numberOperatorOptions: [
        { value: ">=", label: tr("collections.opGe") },
        { value: "<=", label: tr("collections.opLe") },
        { value: ">", label: tr("collections.opGt") },
        { value: "<", label: tr("collections.opLt") },
        { value: "=", label: tr("collections.opEq") },
        { value: "!=", label: tr("collections.opNe") }
    ]

    readonly property var boolOperatorOptions: [
        { value: "=", label: tr("collections.opEq") },
        { value: "!=", label: tr("collections.opNe") }
    ]

    readonly property var sortFieldOptions: [
        { value: "added_at_ms", label: tr("collections.fieldAddedAt") },
        { value: "title", label: tr("collections.fieldTitle") },
        { value: "artist", label: tr("collections.fieldArtist") },
        { value: "album", label: tr("collections.fieldAlbum") },
        { value: "play_count", label: tr("collections.fieldPlayCount") },
        { value: "last_played_at_ms", label: tr("collections.fieldLastPlayedAt") },
        { value: "rating", label: tr("collections.fieldRating") },
        { value: "sample_rate", label: tr("collections.fieldSampleRate") },
        { value: "bit_depth", label: tr("collections.fieldBitDepth") }
    ]

    readonly property var templates: [
        {
            name: tr("collections.templateNone"),
            definition: null,
            sort: null,
            limit: 0
        },
        {
            name: tr("collections.templateRecentlyAdded"),
            definition: { logic: "all", rules: [ { field: "added_days_ago", op: "<=", value: 30 } ] },
            sort: { fields: [ { field: "added_at_ms", dir: "desc" } ] },
            limit: 300
        },
        {
            name: tr("collections.templateFrequentlyPlayed"),
            definition: { logic: "all", rules: [ { field: "play_count", op: ">=", value: 20 } ] },
            sort: { fields: [ { field: "play_count", dir: "desc" } ] },
            limit: 500
        },
        {
            name: tr("collections.templateNeverPlayed"),
            definition: { logic: "all", rules: [ { field: "play_count", op: "=", value: 0 } ] },
            sort: { fields: [ { field: "added_at_ms", dir: "desc" } ] },
            limit: 800
        },
        {
            name: tr("collections.templateHiRes"),
            definition: {
                logic: "any",
                rules: [
                    { field: "bit_depth", op: ">", value: 16 },
                    { field: "sample_rate", op: ">", value: 48000 }
                ]
            },
            sort: { fields: [ { field: "bit_depth", dir: "desc" }, { field: "sample_rate", dir: "desc" } ] },
            limit: 800
        }
    ]

    function operatorOptionsForKind(kind) {
        if (kind === "number") return numberOperatorOptions
        if (kind === "bool") return boolOperatorOptions
        return textOperatorOptions
    }

    function fieldByValue(value) {
        for (let i = 0; i < fieldOptions.length; ++i) {
            if (fieldOptions[i].value === value) {
                return fieldOptions[i]
            }
        }
        return fieldOptions[0]
    }

    function fieldIndex(value) {
        for (let i = 0; i < fieldOptions.length; ++i) {
            if (fieldOptions[i].value === value) {
                return i
            }
        }
        return 0
    }

    function sortFieldIndex(value) {
        for (let i = 0; i < sortFieldOptions.length; ++i) {
            if (sortFieldOptions[i].value === value) {
                return i
            }
        }
        return 0
    }

    function operatorIndex(options, opValue) {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === opValue) {
                return i
            }
        }
        return 0
    }

    function appendRule(fieldValue, opValue, rawValue) {
        const spec = fieldByValue(fieldValue)
        const ops = operatorOptionsForKind(spec.kind)
        const op = opValue && opValue.length > 0 ? opValue : ops[0].value
        rulesModel.append({
            field: spec.value,
            kind: spec.kind,
            op: op,
            valueText: rawValue === undefined || rawValue === null ? "" : String(rawValue),
            valueBool: rawValue === true || rawValue === 1
        })
    }

    function addRule() {
        appendRule("text", "match", "")
        Qt.callLater(function() {
            if (rulesModel.count > 0) {
                rulesList.positionViewAtIndex(rulesModel.count - 1, ListView.End)
            }
        })
    }

    function isValidRuleIndex(ruleIndex) {
        return ruleIndex >= 0 && ruleIndex < rulesModel.count
    }

    function setRuleProperty(ruleIndex, propertyName, propertyValue) {
        if (!isValidRuleIndex(ruleIndex)) {
            return
        }
        rulesModel.setProperty(ruleIndex, propertyName, propertyValue)
    }

    function updateRuleField(ruleIndex, fieldOptionIndex) {
        if (!isValidRuleIndex(ruleIndex)) {
            return
        }
        if (fieldOptionIndex < 0 || fieldOptionIndex >= fieldOptions.length) {
            return
        }

        const spec = fieldOptions[fieldOptionIndex]
        const ops = operatorOptionsForKind(spec.kind)
        setRuleProperty(ruleIndex, "field", spec.value)
        setRuleProperty(ruleIndex, "kind", spec.kind)
        setRuleProperty(ruleIndex, "op", ops[0].value)
        if (spec.kind === "bool") {
            setRuleProperty(ruleIndex, "valueBool", true)
        }
    }

    function updateRuleOperator(ruleIndex, options, optionIndex) {
        if (!isValidRuleIndex(ruleIndex)) {
            return
        }
        if (!options || optionIndex < 0 || optionIndex >= options.length) {
            return
        }
        setRuleProperty(ruleIndex, "op", options[optionIndex].value)
    }

    function updateRuleText(ruleIndex, textValue) {
        if (!isValidRuleIndex(ruleIndex)) {
            return
        }
        const currentRule = rulesModel.get(ruleIndex)
        if (currentRule && currentRule.valueText !== textValue) {
            setRuleProperty(ruleIndex, "valueText", textValue)
        }
    }

    function updateRuleBool(ruleIndex, boolIndex) {
        if (!isValidRuleIndex(ruleIndex)) {
            return
        }
        setRuleProperty(ruleIndex, "valueBool", boolIndex === 0)
    }

    function removeRule(ruleIndex) {
        if (rulesModel.count <= 1 || !isValidRuleIndex(ruleIndex)) {
            return
        }
        rulesModel.remove(ruleIndex)
    }

    function resetForm() {
        errorText = ""
        nameField.text = ""
        logicCombo.currentIndex = 0
        templateCombo.currentIndex = 0
        sortFieldCombo.currentIndex = 0
        sortDirCombo.currentIndex = 1
        limitSpin.value = 0
        enabledCheck.checked = true
        pinnedCheck.checked = false
        rulesModel.clear()
        appendRule("text", "match", "")
    }

    function applyTemplate(index) {
        if (index < 0 || index >= templates.length) {
            return
        }
        const template = templates[index]
        if (!template || !template.definition) {
            return
        }

        rulesModel.clear()
        const templateRules = template.definition.rules || []
        for (let i = 0; i < templateRules.length; ++i) {
            const rule = templateRules[i]
            appendRule(rule.field || "text", rule.op || "match", rule.value)
        }
        if (rulesModel.count === 0) {
            appendRule("text", "match", "")
        }

        logicCombo.currentIndex = (template.definition.logic === "any") ? 1 : 0
        if (template.sort && template.sort.fields && template.sort.fields.length > 0) {
            const primarySort = template.sort.fields[0]
            sortFieldCombo.currentIndex = sortFieldIndex(primarySort.field || "added_at_ms")
            sortDirCombo.currentIndex = (primarySort.dir === "asc") ? 0 : 1
        }
        limitSpin.value = Math.max(0, template.limit || 0)
    }

    function ruleValue(rule) {
        if (rule.kind === "bool") {
            return rule.valueBool ? 1 : 0
        }
        if (rule.kind === "number") {
            const normalized = String(rule.valueText || "").trim()
            const parsed = Number(normalized)
            return Number.isFinite(parsed) ? parsed : 0
        }
        return String(rule.valueText || "").trim()
    }

    function buildPayload() {
        const rules = []
        for (let i = 0; i < rulesModel.count; ++i) {
            const rule = rulesModel.get(i)
            if (!rule.field || !rule.op) {
                continue
            }

            if (rule.kind !== "bool") {
                const valueText = String(rule.valueText || "").trim()
                if (valueText.length === 0) {
                    continue
                }
            }

            rules.push({
                field: rule.field,
                op: rule.op,
                value: ruleValue(rule)
            })
        }

        const logicValue = logicCombo.currentIndex === 1 ? "any" : "all"
        const sortField = sortFieldOptions[sortFieldCombo.currentIndex].value
        const sortDir = sortDirCombo.currentIndex === 0 ? "asc" : "desc"

        const definition = {
            logic: logicValue,
            rules: rules
        }

        const sort = {
            fields: [
                {
                    field: sortField,
                    dir: sortDir
                }
            ]
        }

        return {
            definitionJson: JSON.stringify(definition),
            sortJson: JSON.stringify(sort),
            limitCount: limitSpin.value > 0 ? limitSpin.value : -1,
            valid: rules.length > 0
        }
    }

    function submit() {
        errorText = ""
        if (!smartCollectionsEngine || !smartCollectionsEngine.enabled) {
            errorText = tr("collections.disabled")
            return false
        }

        const name = nameField.text.trim()
        if (name.length === 0) {
            errorText = tr("collections.nameRequired")
            return false
        }

        const payload = buildPayload()
        if (!payload.valid) {
            errorText = tr("collections.rulesRequired")
            return false
        }

        const collectionId = smartCollectionsEngine.createCollection(
            name,
            payload.definitionJson,
            payload.sortJson,
            payload.limitCount,
            enabledCheck.checked,
            pinnedCheck.checked)

        if (collectionId <= 0) {
            errorText = smartCollectionsEngine.lastError.length > 0
                    ? smartCollectionsEngine.lastError
                    : tr("collections.createFailed")
            return false
        }

        close()
        return true
    }

    function openForCreate() {
        resetForm()
        open()
    }

    title: tr("collections.createDialogTitle")
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(760, parent ? parent.width * 0.9 : 760)
    height: Math.min(640, parent ? parent.height * 0.9 : 640)
    standardButtons: Dialog.NoButton

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: themeManager.backgroundColor
        border.width: 1
        border.color: themeManager.borderColor
    }

    onOpened: {
        if (rulesModel.count === 0) {
            resetForm()
        }
    }

    ListModel {
        id: rulesModel
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: topSectionColumn.implicitHeight + 16
                radius: themeManager.borderRadius
                color: themeManager.surfaceColor
                border.width: 1
                border.color: themeManager.borderColor

                ColumnLayout {
                    id: topSectionColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Label {
                        text: dialogRoot.tr("collections.template")
                        color: themeManager.textMutedColor
                        font.pixelSize: 11
                        font.bold: true
                    }

                    AccentComboBox {
                        id: templateCombo
                        Layout.fillWidth: true
                        model: dialogRoot.templates
                        textRole: "name"
                        onActivated: dialogRoot.applyTemplate(currentIndex)
                    }

                    Label {
                        text: dialogRoot.tr("collections.name")
                        color: themeManager.textMutedColor
                        font.pixelSize: 11
                        font.bold: true
                    }

                    TextField {
                        id: nameField
                        Layout.fillWidth: true
                        placeholderText: dialogRoot.tr("collections.namePlaceholder")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: dialogRoot.tr("collections.logic")
                            color: themeManager.textMutedColor
                            font.pixelSize: 11
                        }

                        AccentComboBox {
                            id: logicCombo
                            Layout.preferredWidth: dialogRoot.compactLayout ? 170 : 220
                            model: [dialogRoot.tr("collections.logicAll"), dialogRoot.tr("collections.logicAny")]
                        }

                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        AccentCheckBox {
                            id: enabledCheck
                            text: dialogRoot.tr("collections.enabled")
                            checked: true
                        }

                        AccentCheckBox {
                            id: pinnedCheck
                            text: dialogRoot.tr("collections.pinned")
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: themeManager.borderRadius
                color: themeManager.surfaceColor
                border.width: 1
                border.color: themeManager.borderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: dialogRoot.tr("collections.rules")
                            color: themeManager.textMutedColor
                            font.bold: true
                            font.pixelSize: 11
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: dialogRoot.tr("collections.addRule")
                            icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            onClicked: dialogRoot.addRule()
                        }
                    }

                    ListView {
                        id: rulesList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: rulesModel

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property int index
                            required property string field
                            required property string kind
                            required property string op
                            required property string valueText
                            required property bool valueBool

                            readonly property int ruleIndex: index
                            readonly property var operators: dialogRoot.operatorOptionsForKind(kind)

                            width: rulesList.width - 2
                            height: dialogRoot.compactLayout ? 74 : 42
                            radius: themeManager.borderRadius
                            color: Qt.rgba(themeManager.backgroundColor.r,
                                           themeManager.backgroundColor.g,
                                           themeManager.backgroundColor.b,
                                           0.45)
                            border.width: 1
                            border.color: themeManager.borderColor

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 6
                                visible: !dialogRoot.compactLayout

                                AccentComboBox {
                                    Layout.preferredWidth: 185
                                    model: dialogRoot.fieldOptions
                                    textRole: "label"
                                    currentIndex: dialogRoot.fieldIndex(field)
                                    onActivated: function(activatedIndex) {
                                        dialogRoot.updateRuleField(ruleIndex, activatedIndex)
                                    }
                                }

                                AccentComboBox {
                                    Layout.preferredWidth: 126
                                    model: operators
                                    textRole: "label"
                                    currentIndex: dialogRoot.operatorIndex(operators, op)
                                    onActivated: function(activatedIndex) {
                                        dialogRoot.updateRuleOperator(ruleIndex, operators, activatedIndex)
                                    }
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    visible: kind !== "bool"
                                    text: valueText
                                    placeholderText: dialogRoot.tr("collections.value")
                                    onTextChanged: function() {
                                        dialogRoot.updateRuleText(ruleIndex, text)
                                    }
                                }

                                AccentComboBox {
                                    Layout.fillWidth: true
                                    visible: kind === "bool"
                                    model: [dialogRoot.tr("collections.boolTrue"), dialogRoot.tr("collections.boolFalse")]
                                    currentIndex: valueBool ? 0 : 1
                                    onActivated: function(activatedIndex) {
                                        dialogRoot.updateRuleBool(ruleIndex, activatedIndex)
                                    }
                                }

                                ToolButton {
                                    icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                    enabled: rulesModel.count > 1
                                    onClicked: function() {
                                        dialogRoot.removeRule(ruleIndex)
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6
                                visible: dialogRoot.compactLayout

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    AccentComboBox {
                                        Layout.fillWidth: true
                                        model: dialogRoot.fieldOptions
                                        textRole: "label"
                                        currentIndex: dialogRoot.fieldIndex(field)
                                        onActivated: function(activatedIndex) {
                                            dialogRoot.updateRuleField(ruleIndex, activatedIndex)
                                        }
                                    }

                                    AccentComboBox {
                                        Layout.preferredWidth: 100
                                        model: operators
                                        textRole: "label"
                                        currentIndex: dialogRoot.operatorIndex(operators, op)
                                        onActivated: function(activatedIndex) {
                                            dialogRoot.updateRuleOperator(ruleIndex, operators, activatedIndex)
                                        }
                                    }

                                    ToolButton {
                                        icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                        enabled: rulesModel.count > 1
                                        onClicked: function() {
                                            dialogRoot.removeRule(ruleIndex)
                                        }
                                    }
                                }

                                TextField {
                                    Layout.fillWidth: true
                                    visible: kind !== "bool"
                                    text: valueText
                                    placeholderText: dialogRoot.tr("collections.value")
                                    onTextChanged: function() {
                                        dialogRoot.updateRuleText(ruleIndex, text)
                                    }
                                }

                                AccentComboBox {
                                    Layout.fillWidth: true
                                    visible: kind === "bool"
                                    model: [dialogRoot.tr("collections.boolTrue"), dialogRoot.tr("collections.boolFalse")]
                                    currentIndex: valueBool ? 0 : 1
                                    onActivated: function(activatedIndex) {
                                        dialogRoot.updateRuleBool(ruleIndex, activatedIndex)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: sortSectionColumn.implicitHeight + 16
                radius: themeManager.borderRadius
                color: themeManager.surfaceColor
                border.width: 1
                border.color: themeManager.borderColor

                ColumnLayout {
                    id: sortSectionColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Label {
                        text: dialogRoot.tr("collections.sort")
                        color: themeManager.textMutedColor
                        font.bold: true
                        font.pixelSize: 11
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        AccentComboBox {
                            id: sortFieldCombo
                            Layout.fillWidth: true
                            model: dialogRoot.sortFieldOptions
                            textRole: "label"
                        }

                        AccentComboBox {
                            id: sortDirCombo
                            Layout.preferredWidth: dialogRoot.compactLayout ? 112 : 130
                            model: [dialogRoot.tr("collections.sortAsc"), dialogRoot.tr("collections.sortDesc")]
                            currentIndex: 1
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: dialogRoot.tr("collections.limit")
                            color: themeManager.textMutedColor
                            font.pixelSize: 11
                        }

                        SpinBox {
                            id: limitSpin
                            from: 0
                            to: 50000
                            value: 0
                            editable: true
                            Layout.preferredWidth: dialogRoot.compactLayout ? 108 : 130
                        }

                        Label {
                            text: dialogRoot.tr("collections.limitHint")
                            color: themeManager.textMutedColor
                            font.pixelSize: 10
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: errorLabel.implicitHeight + 12
                radius: themeManager.borderRadius
                visible: dialogRoot.errorText.length > 0
                color: Qt.rgba(0.85, 0.2, 0.2, 0.10)
                border.width: 1
                border.color: Qt.rgba(0.9, 0.35, 0.35, 0.7)

                Label {
                    id: errorLabel
                    anchors.fill: parent
                    anchors.margins: 6
                    text: dialogRoot.errorText
                    color: "#d66"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                Button {
                    id: cancelButton
                    text: dialogRoot.tr("collections.cancel")
                    onClicked: dialogRoot.close()

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: cancelButton.down
                               ? Qt.rgba(themeManager.borderColor.r,
                                         themeManager.borderColor.g,
                                         themeManager.borderColor.b,
                                         0.34)
                               : themeManager.surfaceColor
                        border.width: 1
                        border.color: themeManager.borderColor
                    }

                    contentItem: Label {
                        text: cancelButton.text
                        color: cancelButton.enabled ? themeManager.textColor : themeManager.textMutedColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: themeManager.fontFamily
                    }
                }

                Button {
                    id: createButton
                    text: dialogRoot.tr("collections.create")
                    enabled: nameField.text.trim().length > 0
                    onClicked: dialogRoot.submit()

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: !createButton.enabled
                               ? Qt.rgba(themeManager.primaryColor.r,
                                         themeManager.primaryColor.g,
                                         themeManager.primaryColor.b,
                                         0.32)
                               : (createButton.down
                                  ? Qt.darker(themeManager.primaryColor, 1.16)
                                  : themeManager.primaryColor)
                        border.width: 1
                        border.color: !createButton.enabled
                                      ? Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                0.36)
                                      : Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                0.92)
                    }

                    contentItem: Label {
                        text: createButton.text
                        color: createButton.enabled
                               ? Qt.rgba(themeManager.backgroundColor.r,
                                         themeManager.backgroundColor.g,
                                         themeManager.backgroundColor.b,
                                         0.98)
                               : Qt.rgba(themeManager.backgroundColor.r,
                                         themeManager.backgroundColor.g,
                                         themeManager.backgroundColor.b,
                                         0.62)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: themeManager.fontFamily
                        font.bold: true
                    }
                }
            }
        }
    }
}
