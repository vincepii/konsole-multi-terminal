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

// Qt
#include <QEvent>

// KDE
#include <KDebug>
#include <KXmlGuiWindow>

// Konsole
#include "TerminalDisplay.h"
#include "ViewContainer.h"
#include "SessionController.h"
#include "Session.h"
#include "ViewManager.h"

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
        _childToParent.remove(node);
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

int MultiTerminalDisplayTree::getNumberOfNodes() const
{
    return _childToParent.keys().count();
}

MultiTerminalDisplay* MultiTerminalDisplayTree::getRootNode() const
{
    return _root;
}

// TODO: implement a stack of focused widgets for each tree
//  * when a widget is removed, give focus to the one that had it before
//  * when a different tab is selected, give the focus to the widget that had it

MultiTerminalDisplayManager::MultiTerminalDisplayManager(ViewManager* viewManager
    , QObject* parent /* = 0 */) :
    QObject(parent),
    _viewManager(viewManager),
    UNSPECIFIED_DISTANCE(-1)
{
    // TODO: connect a signal to the viewDestroyed slot, to finish clearing the sessions.
    // also check the other connections of ViewManager::createContainer, as we might need them
    // also put this code every time a mtd is created (helper method)
   connect(this, SIGNAL(viewRemoved(QWidget*)), _viewManager, SLOT(viewDestroyed(QWidget*)));
}

MultiTerminalDisplayManager::~MultiTerminalDisplayManager()
{
    // Nothing to do... hopefully
}

MultiTerminalDisplay* MultiTerminalDisplayManager::createRootTerminalDisplay(TerminalDisplay* terminalDisplay
    , Session* session
    , ViewContainer* container)
{
    // There was no MTD before this one, this is the first insertion
    MultiTerminalDisplay* mtd = new MultiTerminalDisplay(container);

    // We have to start a new tree
    MultiTerminalDisplayTree* mtdTree = new MultiTerminalDisplayTree(mtd);
    _trees.insert(mtd, mtdTree);

    _treeToContainer.insert(mtdTree, container);

    // We want to know when this object will get focus
    mtd->installEventFilter(this);

    combineMultiTerminalDisplayAndTerminalDisplay(mtd, terminalDisplay);

    // The initial split must contain only one child: the terminal display.
    container->hide();

    return mtd;
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
}

MultiTerminalDisplay* MultiTerminalDisplayManager::removeTerminalDisplay(MultiTerminalDisplay* mtd)
{
    // Close the Terminal Display
    TerminalDisplay* removeTd = _mtdContent[mtd];
    removeTd->sessionController()->closeSession();
    emit viewRemoved(removeTd);
    _mtdContent.remove(mtd);

    // Adjust the tree
    MultiTerminalDisplayTree* tree = _trees[mtd];

    // Get the parent of the node to be removed before the tree changes
    MultiTerminalDisplay* parent = tree->getParentOf(mtd);

    // The sibling will take the space of the parent
    MultiTerminalDisplay* sibling = tree->getSiblingOf(mtd);
    // This will change the tree
    tree->removeNode(mtd);

    // Note: sibling is null if we are deleting the root node
    if (sibling != NULL) {
        // Sibling existed, the parent will be the actual parent after the tree has been modified
        MultiTerminalDisplay* newParent = tree->getParentOf(sibling);
        if (newParent == NULL) {
            // The sibling of the node that was removed is now the root node
            ViewContainer* container = _treeToContainer[tree];
            // Note: this is the QT relationship, not the MTDTree.
            sibling->setParent(container);
            // Get the session controller from one of the leaves in the subtree at sibling
            container->addView(sibling, _mtdContent[tree->getLeafOfSubtree(sibling)]->sessionController());
            container->removeView(parent);
        } else {
            // We need to put sibling at the place of its former parent
            // The size of the split
            QSize newParentSize = newParent->size();
            // This is the new node with which we will share the split
            MultiTerminalDisplay* newSibling = tree->getSiblingOf(sibling);
            int newSiblingIndex = newParent->indexOf(newSibling);
            QSize newSiblingSize = newSibling->size();
            // Remove the parent as it will be replaced by the sibling
            parent->setParent(NULL);
            // Respect the position of the existing node (newSiblingIndex)
            newParent->insertWidget(1 - newSiblingIndex ,sibling);
            sibling->setParent(newParent);
            // Resize the widgets
            if (newParent->orientation() == Qt::Horizontal) {
                int siblingWidth = newParentSize.width() - newSiblingSize.width();
                QList<int> sizes;
                sizes.append(siblingWidth);
                // Go before or after depending on the position of the two widgets
                sizes.insert(newSiblingIndex, newSiblingSize.width());
                newParent->setSizes(sizes);
            } else {
                int siblingHeight = newParentSize.height() - newSiblingSize.height();
                QList<int> sizes;
                sizes.append(siblingHeight);
                // Go before or after depending on the position of the two widgets
                sizes.insert(newSiblingIndex, newSiblingSize.height());
                newParent->setSizes(sizes);
            }
        }
        // Set the focus
        setFocusToLeaf(sibling, tree);
    }

    _trees.remove(mtd);
    delete mtd;
    delete parent; // could be null

    return sibling;

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
    // Get all the leaves of all the trees and return every TerminalDisplay
    QList<QWidget*> l;
    foreach (MultiTerminalDisplayTree* tree, getTrees()) {
        QSet<MultiTerminalDisplay*> leaves = tree->getLeaves();
        foreach (MultiTerminalDisplay* leaf, leaves) {
            l.push_back(_mtdContent[leaf]);
        }
    }
    return l;
}

QSet<TerminalDisplay*> MultiTerminalDisplayManager::getTerminalDisplaysOfContainer(MultiTerminalDisplay* multiTerminalDisplay) const
{
    QSet<TerminalDisplay*> tds;
    foreach(MultiTerminalDisplay* mtd, _trees[multiTerminalDisplay]->getLeaves()) {
        tds.insert(_mtdContent[mtd]);
    }
    return tds;
}


TerminalDisplay* MultiTerminalDisplayManager::getTerminalDisplayTo(
    MultiTerminalDisplay* multiTerminalDisplay
    , MultiTerminalDisplayManager::Directions direction
    , MultiTerminalDisplay* treeRoot)
{
    // Get the tree
    MultiTerminalDisplayTree* tree = _trees[treeRoot];
    // TerminalDisplay contained in the current MultiTerminalDisplay
    TerminalDisplay* currentTerminalDisplay = _mtdContent.value(multiTerminalDisplay);
    // Global coordinates (of the top left corner) of the current TerminalDisplay
    QPoint widgetPos = currentTerminalDisplay->mapToGlobal(currentTerminalDisplay->pos());
    // This will store the TerminalDisplay to which we need to move, if any
    TerminalDisplay* moveToTerminalDisplay = 0;
    // Loop through each TerminalDisplay of the same tree and get the one which is closest to
    // the specified direction, if any
    double minDistance = UNSPECIFIED_DISTANCE;
    QSet<MultiTerminalDisplay*> leaves = tree->getLeaves();
    foreach (MultiTerminalDisplay* leaf, leaves) {
        TerminalDisplay* td = _mtdContent[leaf];
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
}

void MultiTerminalDisplayManager::dismissMultiTerminals(MultiTerminalDisplay* multiTerminalDisplay)
{
    MultiTerminalDisplayTree* tree = _trees[multiTerminalDisplay];
    QSet<MultiTerminalDisplay*> leaves = tree->getLeaves();

    while (leaves.size() > 0) {
        MultiTerminalDisplay* newFocusedNode = removeTerminalDisplay(*leaves.begin());
        leaves = tree->getLeaves();
    }

    Q_ASSERT(tree->getNumberOfNodes() == 0);
    Q_ASSERT(_trees.values().contains(tree) == false);

    _treeToContainer.remove(tree);
    delete tree;
}

int MultiTerminalDisplayManager::getNumberOfNodes(MultiTerminalDisplay* mtd) const
{
    MultiTerminalDisplayTree* tree = _trees[mtd];
    return tree->getNumberOfNodes();
}

MultiTerminalDisplay* MultiTerminalDisplayManager::getRootNode(MultiTerminalDisplay* mtd) const
{
    MultiTerminalDisplayTree* tree = _trees[mtd];
    return tree->getRootNode();
}

void MultiTerminalDisplayManager::setFocusForContainer(MultiTerminalDisplay* widget)
{
    // TODO: use a stack of focused widgets
    MultiTerminalDisplayTree* tree = _trees[widget];
    setFocusToLeaf(widget, tree);
}

QSet< MultiTerminalDisplayTree* > MultiTerminalDisplayManager::getTrees() const
{
    return (QSet<MultiTerminalDisplayTree*>::fromList(_trees.values()));
}

bool MultiTerminalDisplayManager::eventFilter(QObject* obj, QEvent* event)
{
    // We install this only on QWidgets
    MultiTerminalDisplay* mtd = qobject_cast<MultiTerminalDisplay*>(obj);

    if (event->type() == QEvent::FocusIn) {
        setFocusForContainer(mtd);
    }

    return QObject::eventFilter(obj, event);
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
