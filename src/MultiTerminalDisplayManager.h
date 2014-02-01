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

typedef QSplitter MultiTerminalDisplay;

// TODO: method to remove all MTDs (root included) to be called e.g. when a tab is closed
// TODO: implementare "Close terminal" solo nel tasto destro, cosi` c-e` sempre la certezza di chi ha
// il focus e non si rischia di chiudere un terminale a caso facendo disastri
// TODO: tree in file separato

/*
 * mappa int-tree* 
 * ogni volta che viene creato un nuovo albero, si genera un id e si aggiunge alla mappa
 * 
 * mappa mtd*-int
 * ogni mtd* e` associato all'indice dell'albero di cui fa parte
 * 
 * mappa int-set<mtd*> set delle foglie per un albero
 * 
 * Implementare class MTDTree
 * 
 * Decisione: c'e` un motivo per non avere direttamente mappa mtd*-tree*, dove ogni mtd e` associato al suo albero?
 * per il momento no, usare questa mappa
 */

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

    /**
     * Constructor for a new tree.
     * 
     * @param rootNode The root node, this will be both root and leaf
     */
    MultiTerminalDisplayTree(MultiTerminalDisplay* rootNode);

    // TODO: cont: implementare classe prendendo funzioni necessarie dal manager

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

private:

    /** With respect to a certain node, its children are a pair of
     * MultiTerminalDisplays */
    typedef QPair<MultiTerminalDisplay*, MultiTerminalDisplay*> MtdTreeChildren;

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
    explicit MultiTerminalDisplayManager(QObject* parent = 0);
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

protected:

    /**
     * Callback for events.
     */
    bool eventFilter(QObject* obj, QEvent* event);

private:

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

    const int UNSPECIFIED_DISTANCE;
};

}
#endif //MULTITERMINALDISPLAYMANAGER_H
