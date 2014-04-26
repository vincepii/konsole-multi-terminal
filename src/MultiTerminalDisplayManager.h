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

#ifndef MULTITERMINALDISPLAYMANAGER_H
#define MULTITERMINALDISPLAYMANAGER_H

// Qt
#include <QtCore/QObject>
#include <QSet>
#include <QSplitter>

namespace Konsole
{

// Konsole
class ViewContainer;
class Session;
class TerminalDisplay;
class ViewManager;

typedef QSplitter MultiTerminalDisplay;

// TODO: method to remove all MTDs (root included) to be called e.g. when a tab is closed
// TODO: implement "Close terminal" on the right click
// TODO: implement the tree class in a different file

/**
 * Properties of this tree:
 * <ul>
 * <li> Each node has only one parent
 * <li> Each node has zero or two children
 * </ul>
 * 
 * The tree doesn't keep ownership of the nodes it is made of, the data
 * structure is only used to keep the relationships between the nodes.
 */
class MultiTerminalDisplayTree {

public:

    /** With respect to a certain node, its children are a pair of
     * MultiTerminalDisplays */
    typedef QPair<MultiTerminalDisplay*, MultiTerminalDisplay*> MtdTreeChildren;

    /**
     * Constructor for a new tree.
     * 
     * @param rootNode The root node, this will be both root and leaf
     */
    MultiTerminalDisplayTree(MultiTerminalDisplay* rootNode);

    /**
     * Returns all the leaves of this tree as a set
     */
    QSet<MultiTerminalDisplay*> getLeaves() const;

    /**
     * Adds to new nodes as children to a parent node.
     */
    void insertNewNodes(MultiTerminalDisplay* parent, MultiTerminalDisplay* child1
                      , MultiTerminalDisplay* child2);

    /**
     * Removes a leaf node from the tree and adjust the tree status.
     * 
     * The current node is deleted from the tree and its sibling will replace
     * their parent.
     * 
     * The algorithm is:
     * * the node that must be deleted (rnode) must be root, or error
     * * if we are removing the root node, we have no tree anymore!
     * * the sibling of rnode will occupy the space that belonged to rnode, so it will
     *   just replace their parent
     * * basically: take rnode out and put the sibling and its subtree at the place
     *   of the parent
     * 
     * If the node is root, it will just be removed.
     */
    void removeNode(MultiTerminalDisplay* node);

    /**
     * Returns the sibling of the given node.
     * 
     * Unless the node is root, there is always a sibling
     */
    MultiTerminalDisplay* getSiblingOf(MultiTerminalDisplay* node);

    /**
     * Returns the parent of the given node (might be null if the node is root)
     */
    MultiTerminalDisplay* getParentOf(MultiTerminalDisplay* node);

    /**
     * Returns true if the given node is root of this tree, false
     * otherwise.
     */
    bool isRoot(MultiTerminalDisplay* node);

    /**
     * Returns a random leaf of the subtree starting at mtd/
     */
    MultiTerminalDisplay* getLeafOfSubtree(MultiTerminalDisplay* mtd) const;

    /**
     * Returns the number of nodes that make up this tree
     */
    int getNumberOfNodes() const;

    /**
     * Returns the root node of this tree
     */
    MultiTerminalDisplay* getRootNode() const;
    
    /**
     * Traverse the MultiTerminalDisplayTree and returns a pointer to the next
     * node.
     * The pointers are yelded as in python "yeld", each invocation of the method
     * yelds another pointer or NULL if all the tree has been traversed.
     * 
     * Tree traversal is done with a stack and it is a depth first traversal.
     * 
     * Note that after traversal has started and before it has completed (i.e.,
     * before the method has returned NULL), the method maintains
     * the internal state of the current traversal, so if a new traversal will
     * start at that point, the behavior will be undefined.
     */
    MultiTerminalDisplay* traverseTreeAndYeldNodes(MultiTerminalDisplay* currentNode);
    
    /**
     * Returns the children of the given node in the tree
     */
    MtdTreeChildren getChildrenOf(MultiTerminalDisplay* node);

private:

    /** Maps each node to its parent */
    QHash<MultiTerminalDisplay*, MultiTerminalDisplay*> _childToParent;

    /** Maps each node to its children (if any) */
    QHash<MultiTerminalDisplay*, MtdTreeChildren> _parentToChildren;

    /** Set of the leaf nodes */
    QSet<MultiTerminalDisplay*> _leaves;

    /** Reference to the root node */
    MultiTerminalDisplay* _root;
};

/**
 * This is a manager of MultiTerminalDisplay objects.
 * 
 * MultiTerminalDisplay are splittable objects that can contain either zero
 * or two MultiTerminalDisplay.
 * 
 * This relationship is represented with a tree of MultiTerminalDisplay
 * objects.
 * 
 * <ul>
 * <li> The root MultiTerminalDisplay has a NULL parent.
 * <li> Each MultiTerminalDisplay is parent of either zero or two
 * MultiTerminalDisplay. Any node in the tree has two children or
 * is a leaf.
 * <li> If a MultiTerminalDisplay is a leaf, then it contains a
 * TerminalDisplay. These are the MultiTerminalDisplay with which the user
 * interacts directly.
 * </ul>
 */
class MultiTerminalDisplayManager : public QObject
{
    Q_OBJECT

public:

enum Directions {
    LEFT = 0,
    RIGHT,
    TOP,
    BOTTOM
};

public:
    explicit MultiTerminalDisplayManager(ViewManager* viewManager, QObject* parent = 0);
    ~MultiTerminalDisplayManager();

    /**
     * Creates a root TerminalDisplay.
     * This method must be used when the first TerminalDisplay
     * must be displayed
     */
    MultiTerminalDisplay* createRootTerminalDisplay(TerminalDisplay* terminalDisplay
        , Session* session
        , ViewContainer* container);
    
    /**
     * This method will promote the currentMultiTerminalDisplay from a leaf
     * to a node with two children.
     * 
     * Two new MultiTerminalDisplay leaves will be prepared, one to host the
     * TerminalDisplay which belonged to the currentMultiTerminalDisplay and
     * another to host the new TerminalDisplay which must be added to the screen.
     * 
     * The currentMultiTerminalDisplay will then become the parent of the new
     * two MultiTerminalDisplays.
     * 
     * \param terminalDisplay The new TerminalDisplay to be added
     * \param session The session that controls the terminalDisplay
     * \param currentMultiTerminalDisplay The MultiTerminalDisplay that must be
     * split in two parts
     * \param orientation The orientation of the split
     */
    void addTerminalDisplay(TerminalDisplay* terminalDisplay
        , Session* session
        , MultiTerminalDisplay* currentMultiTerminalDisplay
        , Qt::Orientation orientation);

    /**
     * Removes the terminaldisplay contained in the given
     * leaf MultiTerminalDisplay
     * 
     * @return The leaf MultiTerminalDisplay that contains the TerminalDisplay
     * that was previously contained in the sibling of the given mtd.
     */
    MultiTerminalDisplay* removeTerminalDisplay(MultiTerminalDisplay* mtd);

    /**
     * Returns the leaf MTD with focus, in the tree starting at the given node
     */
    MultiTerminalDisplay* getFocusedMultiTerminalDisplay(MultiTerminalDisplay* mtd) const;

    /**
     * Returns a list of all the TerminalDisplay owned by this manager.
     */
    QList<QWidget*> getTerminalDisplays() const;

    /**
     * Returns a list of all the TerminalDisplay objects that belong to the same container
     * of the given MultiTerminalDisplay.
     */
    QSet<TerminalDisplay*> getTerminalDisplaysOfContainer(MultiTerminalDisplay* mtd) const;

    /**
     * Given a leaf MultiTerminalDisplay, returns the TerminalDisplay which is
     * closest to that one in the specified direction.
     * 
     * @return The closest TerminalDisplay to the given MultiTerminalDisplay in
     * the specified Direction
     */
    TerminalDisplay* getTerminalDisplayTo(MultiTerminalDisplay* multiTerminalDisplay
                                         , Directions direction
                                         , MultiTerminalDisplay* treeRoot);

    /**
     * Tells if the given MultiTerminalDisplay is a root node.
     */
    bool isRootNode(MultiTerminalDisplay* mtd) const;

    /**
     * Properly shuts down all the terminals, deleting every node
     * of the tree to which the given multiTerminalDisplay belongs.
     * 
     * @param multiTerminalDisplay any MultiTerminalDisplay belonging to
     * view that must be shutdown.
     */
    void dismissMultiTerminals(MultiTerminalDisplay* multiTerminalDisplay);

    /**
     * Given a certain MTD, returns the number of nodes in the tree
     * to which this MTD belongs.
     */
    int getNumberOfNodes(MultiTerminalDisplay* mtd) const;

    /**
     * Given a certain MTD, returns the root node of the tree to which
     * the MTD belongs
     */
    MultiTerminalDisplay* getRootNode(MultiTerminalDisplay* mtd) const;

    /**
     * Clones a MTD and its hierarchy into the given container.
     * 
     * Given a MTD, considers the MTDTree to which that belongs and recreate
     * a new MultiTerminalDisplay that has the same tree and the same terminal
     * sessions.
     * 
     * The logic of the method is as follows:
     * * We traverse the original source tree (the one to be cloned) and keep a pointer
     *   to each one of its nodes
     * * If that node is not a leaf, we create two new nodes and add them to the cloned
     *   node that corresponds to the current one in the source tree
     * * If the node is a leaf, we add to it the terminalDisplay of the node to which
     *   that corresponds in the source tree
     * 
     * @return A MTD that clones the given one
     */
    MultiTerminalDisplay* cloneMtd(MultiTerminalDisplay* sourceMtd, ViewContainer* container);

protected:

    /**
     * Callback for events.
     */
    bool eventFilter(QObject* obj, QEvent* event);

signals:

    void viewRemoved(TerminalDisplay* td);

private:

    /**
     * Private implementation of the addTerminalDisplay method
     */
    void addTerminalDisplay(TerminalDisplay* terminalDisplay
        , Session* session
        , MultiTerminalDisplay* currentMultiTerminalDisplay
        , Qt::Orientation orientation
        , MultiTerminalDisplay* mtd1
        , MultiTerminalDisplay* mtd2);

    /**
     * Puts together a MultiTerminalDisplay and its TerminalDisplay.
     * 
     * Helper method.
     */
    void combineMultiTerminalDisplayAndTerminalDisplay(MultiTerminalDisplay* mtd, TerminalDisplay* td);

    /**
     * Splits the container in two parts of the same size and adds the two widgets.
     */
    void splitMultiTerminalDisplay(MultiTerminalDisplay* container
                                 , MultiTerminalDisplay* widget1, MultiTerminalDisplay* widget2
                                 , Qt::Orientation orientation);

    /**
     * Assigns the focus to a non-specified leaf which belongs to the
     * subtree starting at the given node
     */
    void setFocusToLeaf(MultiTerminalDisplay* mtd, MultiTerminalDisplayTree* tree) const;

    /**
     * Given a MultiTerminalDisplay, sets the focus to one of the
     * TerminalDisplays it contains.
     */
    void setFocusForContainer(MultiTerminalDisplay* widget);

    /**
     * Returns a set of all the trees that are controlled by this manager.
     */
    QSet<MultiTerminalDisplayTree*> getTrees() const;

private:

    /** For each MultiTerminalDisplay tells which is the tree to
     * which it belongs */
    QHash<MultiTerminalDisplay*, MultiTerminalDisplayTree*> _trees;

    /**
     * For each MultiTerminalDisplay, tells what is the contained
     * TerminalDisplay.
     * 
     * Only leave nodes can contain a TerminalDisplay, thus only leaf
     * nodes are key to this Hash.
     */
    QHash<MultiTerminalDisplay*, TerminalDisplay*> _mtdContent;

    /**
     * Maps each tree to the container in which its multi terminals are
     * displayed.
     */
    QHash<MultiTerminalDisplayTree*, ViewContainer*> _treeToContainer;

    /**
     * Reference to the ViewManager which instantiates this object
     */
    ViewManager* _viewManager;

    /**
     * Default distance between multi-terminals, used to implemnt the
     * moveTo[left/up/right/down] shortcuts.
     */
    const int UNSPECIFIED_DISTANCE;
};

}
#endif //MULTITERMINALDISPLAYMANAGER_H
