/*
    This file is part of the Konsole Terminal.

    Copyright 2006-2008 Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "MultiTerminalDisplayManager.h"

// KDE
#include <KDebug>
#include <KXmlGuiWindow>

// Konsole
#include "TerminalDisplay.h"
#include "ViewContainer.h"
#include "SessionController.h"
#include "Session.h"

// Others
#include <math.h>

namespace Konsole
{
QSet< MultiTerminalDisplay* > MultiTerminalDisplayTree::getLeaves() const
{
    return _leaves;
}

void MultiTerminalDisplayTree::insertNewNodes(MultiTerminalDisplay* parent, MultiTerminalDisplay* child1, MultiTerminalDisplay* child2)
{
    // The parent node must be a leaf at this point
    bool parentIsLeaf = _leaves.contains(parent);
    Q_ASSERT(parentIsLeaf);

    if (!parentIsLeaf) {
        kError() << "Parent node must be a leaf node before insertion";
        return;
    }

    _childToParent.insert(child1, parent);
    _childToParent.insert(child2, parent);

    _parentToChildren.insert(parent, qMakePair(child1, child2));

    _leaves.remove(parent);
    _leaves.insert(child1);
    _leaves.insert(child2);
}


MultiTerminalDisplayTree::MultiTerminalDisplayTree(MultiTerminalDisplay* rootNode)
{
    _root = rootNode;
    _childToParent.insert(rootNode, NULL);
    _leaves.insert(rootNode);
}

void MultiTerminalDisplayTree::removeNode(MultiTerminalDisplay* node)
{
    // Node must be a leaf
    bool isLeaf = _leaves.contains(node);
    if (!isLeaf) {
        kError() << "Cannot remove a node which is not a leaf";
        return;
    }

    if (_root == node) {
        // Tree will remain empty!
        _root = NULL;
        _leaves.remove(node);
        return;
    }

    // Not a root leaf

    // Get the parent
    MultiTerminalDisplay* parent = _childToParent[node];
    MultiTerminalDisplay* sibling = getSiblingOf(node);
    MultiTerminalDisplay* gParent = _childToParent[parent];

    // Put the sibling at the place of the parent
    if (gParent == NULL) {
        // We have removed a direct child of the root, the sibling becomes root
        _root = sibling;
    } else {
        // Add the sibling as the child of the gParent
        MultiTerminalDisplay* newSibling = getSiblingOf(parent);
        MtdTreeChildren children = qMakePair(newSibling, sibling);
        _parentToChildren[gParent] = children;
    }

    _childToParent.remove(node);
    _childToParent.remove(sibling);
    _childToParent.remove(parent);
    _parentToChildren.remove(parent);

    _childToParent.insert(sibling, gParent);

    _leaves.remove(node);
}

MultiTerminalDisplay* MultiTerminalDisplayTree::getSiblingOf(MultiTerminalDisplay* node)
{
    if (_root == node) {
        return NULL;
    }

    MultiTerminalDisplay* parent = _childToParent[node];
    MtdTreeChildren children = _parentToChildren[parent];
    return ((node == children.first) ? children.second : children.first);
}

MultiTerminalDisplay* MultiTerminalDisplayTree::getParentOf(MultiTerminalDisplay* node)
{
    return _childToParent[node];
}

bool MultiTerminalDisplayTree::isRoot(MultiTerminalDisplay* node)
{
    return _root == node;
}

MultiTerminalDisplay* MultiTerminalDisplayTree::getLeafOfSubtree(MultiTerminalDisplay* mtd) const
{
    QHash<MultiTerminalDisplay*, MtdTreeChildren>::const_iterator it = _parentToChildren.find(mtd);
    while (it != _parentToChildren.end()) {
        mtd = it->first;
        it = _parentToChildren.find(mtd);
    }
    return mtd;
}

MultiTerminalDisplayManager::MultiTerminalDisplayManager(QObject* parent /* = 0 */) :
    QObject(parent),
    UNSPECIFIED_DISTANCE(-1)
{
}

MultiTerminalDisplayManager::~MultiTerminalDisplayManager()
{
    // TODO
    // Destroy all the new objects!!
}

MultiTerminalDisplay* MultiTerminalDisplayManager::createRootTerminalDisplay(TerminalDisplay* terminalDisplay
    , Session* session
    , ViewContainer* container)
{
    // There was no MTD before this one, this is the first insertion
    MultiTerminalDisplay* mtd = new MultiTerminalDisplay;

    // We have to start a new tree
    MultiTerminalDisplayTree* mtdTree = new MultiTerminalDisplayTree(mtd);
    _trees.insert(mtd, mtdTree);

    combineMultiTerminalDisplayAndTerminalDisplay(mtd, terminalDisplay);

    return mtd;

// XXX old code
//     // There was no MTD before this one, this is the first insertion
//     MultiTerminalDisplay* mtd = new MultiTerminalDisplay;
//     // Root MTD has null parent
//     _mtdTree.insert(mtd, 0);
//     _mtdContent.insert(mtd, terminalDisplay);
//     _displays.insert(session, terminalDisplay);
//     terminalDisplay->setParent(mtd);
//     mtd->addWidget(terminalDisplay);
//     _leaves.insert(mtd);
//     return mtd;
}

void MultiTerminalDisplayManager::addTerminalDisplay(TerminalDisplay* terminalDisplay
    , Session* session
    , MultiTerminalDisplay* currentMultiTerminalDisplay
    , Qt::Orientation orientation)
{
    // Get the tree
    MultiTerminalDisplayTree* mtdTree = _trees[currentMultiTerminalDisplay];

    // Create two new MTD, one for the existing TD and one for the new TD
    MultiTerminalDisplay* mtd1 = new MultiTerminalDisplay(currentMultiTerminalDisplay);
    MultiTerminalDisplay* mtd2 = new MultiTerminalDisplay(currentMultiTerminalDisplay);
    mtdTree->insertNewNodes(currentMultiTerminalDisplay, mtd1, mtd2);

    _trees.insert(mtd1, mtdTree);
    _trees.insert(mtd2, mtdTree);

    // Current TerminalDisplay
    TerminalDisplay* currentTd = _mtdContent[currentMultiTerminalDisplay];
    _mtdContent.remove(currentMultiTerminalDisplay);
    combineMultiTerminalDisplayAndTerminalDisplay(mtd1, currentTd);
    combineMultiTerminalDisplayAndTerminalDisplay(mtd2, terminalDisplay);
    splitMultiTerminalDisplay(currentMultiTerminalDisplay, mtd1, mtd2, orientation);

    terminalDisplay->setFocus();




// XXX old code
//     Q_ASSERT(currentMultiTerminalDisplay != 0);
//     // The current MTD is a leaf (will be promoted to non-leaf after insertion)
//     Q_ASSERT(_mtdContent.values().contains(currentMultiTerminalDisplay) == false);
//     // Create two new MTD, one for the existing TD and one for the new TD
//     MultiTerminalDisplay* mtd1 = new MultiTerminalDisplay(currentMultiTerminalDisplay);
//     MultiTerminalDisplay* mtd2 = new MultiTerminalDisplay(currentMultiTerminalDisplay);
//     // Current TerminalDisplay
//     TerminalDisplay* currentTd = _mtdContent[currentMultiTerminalDisplay];
//     Q_ASSERT(currentTd);
//     // This MTD will not be a leaf anymore and only leaves contain TerminalDisplays
//     int numberOfTerminalDisplays = _mtdContent.remove(currentMultiTerminalDisplay);
//     Q_ASSERT(numberOfTerminalDisplays == 1);
//     // Hierarchy
//     _mtdTree.insert(mtd1, currentMultiTerminalDisplay);
//     _mtdTree.insert(mtd2, currentMultiTerminalDisplay);
//     // TD holding
//     _mtdContent.insert(mtd1, currentTd);
//     _mtdContent.insert(mtd2, terminalDisplay);
//     // Session
//     _displays.insert(session, terminalDisplay);
//     // Actual display
//     mtd1->addWidget(currentTd);
//     mtd2->addWidget(terminalDisplay);
//     // Replication, get size of the current MTD
//     QList<int> sizes = currentMultiTerminalDisplay->sizes();
//     QList<int> childSizes;
//     childSizes.append(sizes.at(0) / 2);
//     childSizes.append(sizes.at(0) / 2);
//     currentMultiTerminalDisplay->setOrientation(orientation);
//     currentMultiTerminalDisplay->addWidget(mtd1);
//     currentMultiTerminalDisplay->addWidget(mtd2);
//     currentTd->setParent(mtd1);
//     terminalDisplay->setParent(mtd2);
//     currentMultiTerminalDisplay->setSizes(childSizes);
//     // Update the leaves status
//     _leaves.remove(currentMultiTerminalDisplay);
//     _leaves.insert(mtd1);
//     _leaves.insert(mtd2);
//     // Set the focus to the new terminal display
//     terminalDisplay->setFocus();
}

MultiTerminalDisplay* MultiTerminalDisplayManager::removeTerminalDisplay(MultiTerminalDisplay* mtd)
{
    // Close the Terminal Display
    TerminalDisplay* removeTd = _mtdContent[mtd];
    removeTd->sessionController()->closeSession();
    _mtdContent.remove(mtd);

    // Adjust the tree
    MultiTerminalDisplayTree* tree = _trees[mtd];
    // The sibling will take the space of the parent
    MultiTerminalDisplay* sibling = tree->getSiblingOf(mtd);
    tree->removeNode(mtd);

    if (sibling == NULL) {
        // We are removing the root node
        delete tree;
        mtd->setParent(NULL);
        delete mtd;
        return NULL;
    }

    // Sibling existed, the parent will be the actual parent after the tree has been modified
    MultiTerminalDisplay* newParent = tree->getParentOf(sibling);
    // Note, this is the QT relationship!
    if (newParent != NULL) {
        // Only if sibling is not root
        sibling->setParent(newParent);
    }

    // Set the focus
    setFocusToLeaf(sibling, tree);

    _trees.remove(mtd);
    delete mtd;
    mtd = NULL;

    return sibling;








// XXX old code
//     // This must be a leaf node...
//     bool isLeaf = _leaves.contains(mtd);
//     Q_ASSERT(isLeaf);
// 
//     if (!isLeaf) {
//         kError() << "Node to be removed must be a leaf";
//         return NULL;
//     }
// 
//     // This will point to the new leaf node that will have focus after the mtd is removed
//     MultiTerminalDisplay* newLeaf = 0;
// 
//     // The node is a leaf
// 
//     // Clean the terminaldisplay
//     TerminalDisplay* td = _mtdContent[mtd];
//     Session* session = td->sessionController()->session().data();
//     SessionController* controller = td->sessionController();
//     controller->closeSession();
// 
//     // Take the parent node
//     MultiTerminalDisplay* parent = _mtdTree[mtd];
//     if (parent == 0) {
//         // This is root and leaf, we are closing the last terminal
//         td->setParent(0);
//         newLeaf = 0;
//     } else {
//         // This is not root, the parent will become leaf and take the
//         // TerminalDisplay of the other son, which will disappear as well
// 
//         // Get the sibling
//         MultiTerminalDisplay* sibling = getSiblingOf(mtd);
//         // Get the sibling's TerminalDisplay
//         TerminalDisplay* siblingTd = _mtdContent[sibling];
//         // This TD will be owned by the parent
//         parent->addWidget(siblingTd);
//         siblingTd->setParent(parent);
//         sibling->setParent(0);
//         _mtdContent[parent] = siblingTd;
//         _leaves.insert(parent);
//         // Set the focus
//         siblingTd->setFocus();
//         // This will be the new leaf after the given mtd is removed
//         newLeaf = parent;
// 
//         _leaves.remove(sibling);
//         _mtdContent.remove(sibling);
//         _mtdTree.remove(sibling);
// 
//         delete sibling;
//         sibling = 0;
//     }
// 
//     // Cleanup the data structures
//     _mtdContent.remove(mtd);
//     _mtdTree.remove(mtd);
//     _displays.remove(session);
//     _leaves.remove(mtd);
// 
//     delete mtd;
//     mtd = 0;
// 
//     return newLeaf;
//     // TODO: the session has changed, the new one should be set as the current one somewhere?
}

void MultiTerminalDisplayManager::setFocusToLeaf(MultiTerminalDisplay* mtd, MultiTerminalDisplayTree* tree) const
{
    MultiTerminalDisplay* leaf = tree->getLeafOfSubtree(mtd);
    _mtdContent[leaf]->setFocus();
}

MultiTerminalDisplay* MultiTerminalDisplayManager::getFocusedMultiTerminalDisplay(MultiTerminalDisplay* mtd) const
{
    MultiTerminalDisplayTree* tree = _trees[mtd];

    if (tree == NULL) {
        kError() << "Provided MultiTerminalDisplay doesn't belong to any tree";
        return NULL;
    }

    // Get the leavs of this tree, only leaves can have focus
    foreach (MultiTerminalDisplay* mtd, tree->getLeaves()) {
        if (_mtdContent[mtd]->hasFocus()) {
            return mtd;
        }
    }

    // Error condition
    kError() << "No leaf has focus";
    return NULL;
}

QList<QWidget*> MultiTerminalDisplayManager::getTerminalDisplays() const
{
    // TODO: cont refactoring from this method
    QList<QWidget*> l;
    foreach (QWidget* td, _displays.values()) {
        l.push_back(td);
    }
    return l;
}

TerminalDisplay* MultiTerminalDisplayManager::getTerminalDisplayTo(
    MultiTerminalDisplay* multiTerminalDisplay
    , MultiTerminalDisplayManager::Directions direction)
{
    // TODO: error, if there are multiple tabs, we go to the mtd of another tab...
    //       we need to separate the trees of mtds.
    // Assert that this is a leaf node
    Q_ASSERT(_mtdContent.contains(multiTerminalDisplay));
    // TerminalDisplay contained in the current MultiTerminalDisplay
    TerminalDisplay* currentTerminalDisplay = _mtdContent.value(multiTerminalDisplay);
    // Global coordinates (of the top left corner) of the current TerminalDisplay
    QPoint widgetPos = currentTerminalDisplay->mapToGlobal(currentTerminalDisplay->pos());
    // This will store the TerminalDisplay to which we need to move, if any
    TerminalDisplay* moveToTerminalDisplay = 0;
    // Loop through each TerminalDisplay and get the one which is closest to
    // the specified direction, if any
    double minDistance = UNSPECIFIED_DISTANCE;
    foreach (TerminalDisplay* td, _mtdContent.values()) {
        QPoint tdPoint = td->mapToGlobal(td->pos());
        // Coordinates of the current TerminalDisplay
        int x = tdPoint.x();
        int y = tdPoint.y();
        if ((direction == LEFT && x < widgetPos.x())
            || (direction == TOP && y < widgetPos.y())
            || (direction == RIGHT && x > widgetPos.x())
            || (direction == BOTTOM && y > widgetPos.y())) {
            // This is in the right direction, get the distance
            double distance = sqrt(pow(widgetPos.x() - x, 2) + pow(widgetPos.y() - y, 2));
            if (distance < minDistance || minDistance == UNSPECIFIED_DISTANCE) {
                // This the new closest widget above our widget
                moveToTerminalDisplay = td;
                minDistance = distance;
            }
        }
    }
    return moveToTerminalDisplay;
}

bool MultiTerminalDisplayManager::isRootNode(MultiTerminalDisplay* mtd) const
{
    MultiTerminalDisplayTree* tree = _trees[mtd];

    if (tree == NULL) {
        kError() << "Provided MultiTerminalDisplay doesn't belong to any tree";
    }

    return tree->isRoot(mtd);

// XXX old code

//     QHash<MultiTerminalDisplay*, MultiTerminalDisplay*>::const_iterator it = _mtdTree.find(mtd);
//     Q_ASSERT(it != _mtdTree.end());
// 
//     if (it != _mtdTree.end()) {
//         return it.value() == 0;
//     }
// 
//     kWarning() << "Given node is not in the tree of MultiTerminalDisplay";
//     return false;
}

void MultiTerminalDisplayManager::dismissMultiTerminals(MultiTerminalDisplay* multiTerminalDisplay)
{
    bool isLeaf = _leaves.contains(multiTerminalDisplay);
    Q_ASSERT(isleaf);

    if (!isLeaf) {
        kError() << "Wrong argument: object is not a leaf";
    }

    MultiTerminalDisplay* deleteMtd = multiTerminalDisplay;
    while (_mtdTree[deleteMtd] != 0) {
        // Node is not root, remove any node up to the root
        deleteMtd = removeTerminalDisplay(deleteMtd);
    }
}


MultiTerminalDisplay* MultiTerminalDisplayManager::getSiblingOf(MultiTerminalDisplay* multiTerminalDisplay)
{
    MultiTerminalDisplay* parent = _mtdTree[multiTerminalDisplay];
    Q_ASSERT(parent != 0);

    if (parent == 0) {
        // The given mtd is root
        return 0;
    }

    foreach (MultiTerminalDisplay* mtd , _mtdTree.keys()) {
        if (_mtdTree.value(mtd) == parent && mtd != multiTerminalDisplay) {
            return mtd;
        }
    }

    // Not found
    return 0;
}

void MultiTerminalDisplayManager::combineMultiTerminalDisplayAndTerminalDisplay(MultiTerminalDisplay* mtd, TerminalDisplay* td)
{
    _mtdContent.insert(mtd, td);
    mtd->addWidget(td);
    td->setParent(mtd);
}

void MultiTerminalDisplayManager::splitMultiTerminalDisplay(MultiTerminalDisplay* container
                                                 , MultiTerminalDisplay* widget1
                                                 , MultiTerminalDisplay* widget2
                                                 , Qt::Orientation orientation)
{
    // Get the sizes
    QList<int> sizes = container->sizes();
    QList<int> childSizes;
    childSizes.append(sizes.at(0) / 2);
    childSizes.append(sizes.at(0) / 2);
 
    // Split
    container->setOrientation(orientation);
    container->addWidget(widget1);
    container->addWidget(widget2);
    container->setSizes(childSizes);
}


}
