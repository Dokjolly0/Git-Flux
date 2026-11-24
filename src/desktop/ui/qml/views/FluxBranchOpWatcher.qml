import QtQuick

Connections {
    id: watcher

    property var branchManager: null
    property int pendingOperationId: 0

    signal succeeded()
    signal failed(string message)

    target: watcher.branchManager
    enabled: watcher.pendingOperationId !== 0

    function onOperationFinished(operationId, success, message) {
        if (operationId !== watcher.pendingOperationId) return;
        watcher.pendingOperationId = 0;
        if (success) {
            watcher.succeeded();
        } else {
            watcher.failed(message);
        }
    }
}
