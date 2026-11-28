import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Modifica Messaggio Commit"
    modal: true
    width: Math.min(580, Overlay.overlay ? Overlay.overlay.width - 48 : 580)
    padding: 18
    topPadding: 16
    bottomPadding: 16
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    property var branchManager: null
    property string targetSha: ""
    property string originalSubject: ""
    property string originalBody: ""
    property string authorName: ""
    property string commitDate: ""
    property bool isHead: false

    readonly property bool isSaving: opWatcher.pendingOperationId !== 0
    readonly property string trimmedSubject: subjectField.text.trim()
    readonly property string normalizedBody: bodyArea.text.replace(/\s+$/, "")
    readonly property bool isOverRecommendedLength: subjectField.text.length > 72
    readonly property bool canSubmit: trimmedSubject.length > 0
                                      && !isSaving
                                      && !(branchManager && branchManager.isBusy)

    function openForCommit(commit, headFlag) {
        if (!commit) {
            return;
        }
        root.targetSha = commit.sha || "";
        root.originalSubject = commit.subject || "";
        root.originalBody = (commit.body || "").replace(/\s+$/, "");
        root.authorName = commit.author || "";
        root.commitDate = commit.date || "";
        root.isHead = Boolean(headFlag);

        subjectField.text = root.originalSubject;
        bodyArea.text = root.originalBody;
        errorText.text = "";

        root.open();
        subjectField.forceActiveFocus();
        subjectField.selectAll();
    }

    function submit() {
        if (!root.canSubmit || !root.branchManager || root.targetSha.length === 0) {
            return;
        }

        const newMessage = root.normalizedBody.length > 0
            ? (root.trimmedSubject + "\n\n" + root.normalizedBody)
            : root.trimmedSubject;
        const origMessage = root.originalBody.length > 0
            ? (root.originalSubject + "\n\n" + root.originalBody)
            : root.originalSubject;

        if (newMessage === origMessage) {
            root.close();
            return;
        }

        errorText.text = "";
        if (root.branchManager.rewordCommit(root.targetSha, newMessage)) {
            opWatcher.pendingOperationId = root.branchManager.lastOperationId;
        } else {
            errorText.text = root.branchManager.statusMessage || "Impossibile avviare la modifica del commit.";
        }
    }

    FluxBranchOpWatcher {
        id: opWatcher
        branchManager: root.branchManager
        onSucceeded: root.close()
        onFailed: (message) => {
            errorText.text = message && message.length > 0
                ? message
                : "Errore durante la riscrittura del commit.";
        }
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        implicitHeight: 46
        titleText: "✏️ Modifica Messaggio Commit"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 14

        // Commit identity banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            implicitHeight: 44
            radius: 10
            color: "#0D131D"
            border.color: "#222F42"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Rectangle {
                    Layout.preferredHeight: 22
                    implicitWidth: shaText.implicitWidth + 14
                    implicitHeight: 22
                    radius: 6
                    color: Qt.alpha("#A855F7", 0.16)
                    border.color: Qt.alpha("#A855F7", 0.55)
                    border.width: 1

                    Text {
                        id: shaText
                        anchors.centerIn: parent
                        text: root.targetSha.length > 0 ? root.targetSha.substring(0, 7) : "HEAD"
                        color: "#D8B4FE"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "monospace"
                    }
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    implicitWidth: scopePillText.implicitWidth + 12
                    implicitHeight: 22
                    radius: 6
                    color: root.isHead ? Qt.alpha("#38BDF8", 0.14) : Qt.alpha("#F59E0B", 0.14)
                    border.color: root.isHead ? Qt.alpha("#38BDF8", 0.45) : Qt.alpha("#F59E0B", 0.45)
                    border.width: 1

                    Text {
                        id: scopePillText
                        anchors.centerIn: parent
                        text: root.isHead ? "HEAD (Amend)" : "Rebase Branch"
                        color: root.isHead ? "#7DD3FC" : "#FCD34D"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Text {
                    text: root.authorName.length > 0
                        ? (root.authorName + (root.commitDate.length > 0 ? (" · " + root.commitDate) : ""))
                        : root.commitDate
                    color: "#94A3B8"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        // Title field + live character counter
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "Titolo del commit"
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredHeight: 20
                    implicitWidth: charCountText.implicitWidth + 12
                    implicitHeight: 20
                    radius: 10
                    color: root.isOverRecommendedLength
                        ? Qt.alpha("#F59E0B", 0.16)
                        : Qt.alpha("#64748B", 0.16)
                    border.color: root.isOverRecommendedLength
                        ? Qt.alpha("#F59E0B", 0.55)
                        : Qt.alpha("#64748B", 0.35)
                    border.width: 1

                    Text {
                        id: charCountText
                        anchors.centerIn: parent
                        text: root.isOverRecommendedLength
                            ? (subjectField.text.length + " / 72 (consigliati ≤ 72)")
                            : (subjectField.text.length + " / 72 caratteri")
                        color: root.isOverRecommendedLength ? "#FCD34D" : "#94A3B8"
                        font.pixelSize: 10
                        font.bold: root.isOverRecommendedLength
                    }
                }
            }

            TextField {
                id: subjectField
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                implicitHeight: 38
                color: "#F8FAFC"
                font.pixelSize: 13
                font.bold: true
                placeholderText: "Es. feat(ui): migliora layout modale commit"
                placeholderTextColor: "#64748B"
                selectByMouse: true
                enabled: !root.isSaving

                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: subjectField.activeFocus
                        ? "#A855F7"
                        : (root.isOverRecommendedLength ? Qt.alpha("#F59E0B", 0.55) : "#222F42")
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation { duration: 120 }
                    }
                }

                Keys.onReturnPressed: bodyArea.forceActiveFocus()
                Keys.onEnterPressed: bodyArea.forceActiveFocus()
            }
        }

        // Description field
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "Descrizione"
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    font.bold: true
                }

                Text {
                    text: "(opzionale · dettagli e motivazione)"
                    color: "#64748B"
                    font.pixelSize: 11
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 144
                implicitHeight: 144
                color: "#0A0E15"
                radius: 8
                border.color: bodyArea.activeFocus ? "#A855F7" : "#222F42"
                border.width: 1

                Behavior on border.color {
                    ColorAnimation { duration: 120 }
                }

                ScrollView {
                    SmoothWheelScroller {}
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    ScrollBar.vertical: FluxScrollBar {}

                    TextArea {
                        id: bodyArea
                        placeholderText: "Aggiungi una descrizione estesa delle modifiche..."
                        placeholderTextColor: "#64748B"
                        color: "#E2E8F0"
                        font.pixelSize: 12
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        background: null
                        enabled: !root.isSaving

                        Keys.onPressed: function (event) {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                    && (event.modifiers & Qt.ControlModifier) !== 0) {
                                root.submit();
                                event.accepted = true;
                            }
                        }
                    }
                }
            }
        }

        // History rewrite notice
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warnRow.implicitHeight + 16
            implicitHeight: warnRow.implicitHeight + 16
            radius: 9
            color: root.isHead ? "#131C2B" : "#251A09"
            border.color: root.isHead ? "#263B59" : "#92400E"
            border.width: 1

            RowLayout {
                id: warnRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: root.isHead ? "ℹ️" : "⚠️"
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignTop
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: root.isHead ? "#93C5FD" : "#FCD34D"
                    text: root.isHead
                        ? "Questo è l'ultimo commit del branch (HEAD): verrà eseguito un amend generando un nuovo SHA. Se è già stato pushato servirà un force push."
                        : "Questo commit precede altri commit del branch: verrà eseguito un rebase che aggiornerà gli SHA di questo commit e di tutti i successivi (i file non cambiano). Se già pushati servirà un force push."
                }
            }
        }

        // Error feedback
        Text {
            id: errorText
            Layout.fillWidth: true
            color: "#F87171"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }
    }

    footer: Rectangle {
        color: "#161F2E"
        implicitHeight: 54
        height: 54
        radius: 14

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 14
            color: "#161F2E"
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: "#222F42"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 10

            Text {
                text: "Ctrl+Invio per salvare · Esc per annullare"
                color: "#64748B"
                font.pixelSize: 11
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 34
                fontPixelSize: 12
                enabled: !root.isSaving
                onClicked: root.close()
            }

            FluxActionButton {
                text: root.isSaving
                    ? "Salvataggio..."
                    : (root.isHead ? "✓ Salva Modifiche" : "✓ Salva e Riscrivi")
                Layout.preferredHeight: 34
                enabled: root.canSubmit
                baseColor: "#7E22CE"
                hoverColor: "#9333EA"
                baseBorderColor: "#A855F7"
                textColor: "#FFFFFF"
                fontPixelSize: 12
                fontBold: true
                onClicked: root.submit()
            }
        }
    }
}
