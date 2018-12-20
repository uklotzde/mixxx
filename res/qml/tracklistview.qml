import QtQuick 2.0
import QtQuick.Controls 2.2

Item {

    Component {
        id: trackItemDelegate
        Text {
            id: item
            width: parent.width
            text: artist + " - " + title
            MouseArea {
                anchors.fill: parent
                onClicked: item.ListView.view.currentIndex = index
            }
        }
    }

    ListView {
        id: trackListView
        model: trackListModel
        delegate: trackItemDelegate

        anchors.fill: parent
        focus: true
        clip: true

        highlight: Rectangle { color: "lightsteelblue"; radius: 3 }
        highlightMoveDuration: 0

        ScrollBar.horizontal: ScrollBar { id: hbar; active: vbar.active }
        ScrollBar.vertical: ScrollBar { id: vbar; active: hbar.active }
    }

}