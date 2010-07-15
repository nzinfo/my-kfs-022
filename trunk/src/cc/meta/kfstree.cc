/*!
 * $Id: kfstree.cc 71 2008-07-07 15:49:14Z sriramsrao $
 *
 * \file kfstree.cc
 * \brief Tree-manipulating routines for the KFS metadata server.
 * \author Blake Lewis (Kosmix Corp.)
 *
 * Copyright 2008 Quantcast Corp.
 * Copyright 2006-2008 Kosmix Corp.
 *
 * This file is part of Kosmos File System (KFS).
 *
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "kfstree.h"
#include "checkpoint.h"

using std::for_each;

using namespace KFS;

Tree KFS::metatree;

/*!
 * \brief Insert a child node at the indicated position.
 * \param[in] child	the node to be inserted
 * \param[in] pos	the index where it should go
 */
// 向孩子节点中指定的位置添加一个孩子
void Node::addChild(Key *k, MetaNode *child, int pos)
{
	// 将指定的位置空出来
	openHole(pos, 1);
	childKey[pos] = *k;
	childNode[pos] = child;
}

/*!
 * \brief remove child and shift remaining entries to fill in the hole
 * \param[in] pos index of the child node
 *
 */
/// 删除指定的孩子节点，并将它从树中移除
void Node::remove(int pos)
{
	assert(pos >= 0 && pos < count);
	Meta *m = leaf(pos);
	delete m;
	closeHole(pos, 1);
}

/// 将本地从start开始的n个节点移动到指定的Node下
void Node::moveChildren(Node *dest, int start, int n)
{
	for (int i = 0; i != n; i++)
		dest->appendChild(childKey[start + i], childNode[start + i]);
	childKey[start] = Key(KFS_SENTINEL, 0);
	childNode[start] = NULL;
}

/*!
 * \brief split a full node
 * \param[in] t	the tree (in case we add a new root)
 * \param[in] father	the parent of this node
 * \param[in] pos	position of this node in parent
 * \return	pointer to newly constructed sibling node
 *
 * Split this node (which is assumed to be full) into two
 * pieces, adding a new pointer to the father node, and if
 * necessary, a new root to the tree.  We split full nodes
 * as we traverse the tree downwards, so it should never
 * happen that the father node is full at this point.
 */
/// 为方便操作，指定该Tree的指针，该Node的父亲指针以及该Node是father节点的第几个孩子
Node *Node::split(Tree *t, Node *father, int pos)
{
	Node *brother = new Node(flags());

	// 将两个节点连接起来，各自的next指针分别指向对方
	brother->linkToPeer(next);
	linkToPeer(brother);

	// 将当前节点的后几个孩子移动到新生成的节点上(append)
	moveChildren(brother, count - NSPLIT, NSPLIT);
	count -= NSPLIT;	// ----> 这个可以封装在moveChildren中
	if (father == NULL)
	{ // this must be the root
		// 如果这个节点是根节点，则将新生成的节点加入根节点
		assert(t->getroot() == this);

		// 生成新的root，将原root和brother加入新root的孩子目录
		t->pushroot(brother);
	}
	else
	{
		// 这个节点不是根节点，则father节点一定不满(因为在插入的操作当中，自顶向下的所有
		// 满节点都已经被分割了)
		assert(!father->isfull());
		assert(father->child(pos) == this);
		father->resetKey(pos);
		Key k = brother->key();
		// 将新生成的节点加入到原节点的下一个孩子位置
		father->addChild(&k, brother, 1 + pos);
	}

	return brother;
}

/*
 * Create a space in the link array by moving everything
 * with index >= _pos_ by _skip_ spaces to the right.
 * N.B. Node must not be full
 */
/// 将孩子节点中从第pos个位置开始的skip个位置空出来：将这个位置开始的元素依次向后移动
void Node::openHole(int pos, int skip)
{
	count += skip;
	assert(count <= NKEY);
	for (int i = count - 1; i >= pos + skip; --i)
	{
		childKey[i] = childKey[i - skip];
		childNode[i] = childNode[i - skip];
	}
}

/*
 * Fill in a hole created by moving or deleting child pointers.
 * pos is the beginning of the hole and skip is its size.
 */
// 删除从pos开始的skip个孩子，并将后边的孩子以此向前移动，补齐空间
void Node::closeHole(int pos, int skip)
{
	assert(skip < count);
	count -= skip;
	for (int i = pos; i != count; i++)
	{
		childKey[i] = childKey[i + skip];
		childNode[i] = childNode[i + skip];
	}
	childKey[count] = Key(KFS_SENTINEL, 0);
	childNode[count] = NULL;
}

/*
 * Insert a new metadata item at the specified position
 * in this node, after making space by moving everything
 * with index >= pos to the right.
 */
/// 在Node的pos位置插入item元素
void Node::insertData(Key *k, Meta *item, int pos)
{
	assert(hasleaves());
	addChild(k, item, pos);
}

/*
 * This node is absorbed by its left neighbor.
 */
/// 将当前节点的所有孩子移动到l中
/// @note 动作之后，本节点中的内容变成了空的
void Node::absorb(Node *l)
{
	assert(count + l->children() <= NKEY);
	moveChildren(l, 0, count);
	count = 0;
	l->next = next;
}

/*!
 * \brief join an underfull node with its neighbor if possible
 * \param[in] pos	position of underfull child
 * \return		true if operation succeeded
 *
 * If the underfull child node at position pos has a
 * neighbor on either side that can absorb it, combine
 * the two nodes into one.
 *
 * Don't do the merge if the resulting node would be completely
 * full, since in that case, balanceNeighbor is probably a better
 * remedy.
 *
 * Return true if merge took place.
 */
/// 将第pos个孩子同它的邻居合并：如果左节点可以合并，则将本节点合并到左节点中；否则，合并到右
/// 兄弟节点中
/// @note 必须保证合并后，总节点数不大于NKEY
bool Node::mergeNeighbor(int pos)
{
	assert(!hasleaves());
	Node *left = leftNeighbor(pos);
	Node *right = rightNeighbor(pos);
	Node *middle = child(pos);
	int mkids = middle->children();
	int base;

	if (left != NULL && mkids + left->children() < NKEY)
	{
		middle->absorb(left);
		base = pos - 1;
	}
	else if (right != NULL && mkids + right->children() < NKEY)
	{
		right->absorb(middle);
		base = pos;
	}
	else
		return false;

	childKey[base] = childKey[base + 1];
	delete childNode[base + 1];
	// 删除空余出来的节点位置
	closeHole(base + 1, 1);

	return true;
}

/*
 * Move n children from _start_ in this node to the
 * beginning of _dest.
 */
/// 将本地从start指定位置开始的n个孩子替换dest开头的n个孩子
void Node::insertChildren(Node *dest, int start, int n)
{
	count -= n;	/// 这样做最后的几个孩子不都无法访问了???
	for (int i = 0; i != n; i++)
		dest->placeChild(childKey[start + i], childNode[start + i], i);
}

/*
 * Move nshift children from the right end of this node
 * to the beginning of _dest_.
 */
/// 将本地末尾的nshift个孩子移动到dest的开头nshift个位置处
void Node::shiftRight(Node *dest, int nshift)
{
	dest->openHole(0, nshift);
	insertChildren(dest, count - nshift, nshift);
}

/*
 * Move nshift children from the beginning of this node
 * to the end of _dest_.
 */
/// 将本地开头的nshift个孩子节点移动到dest的末尾
void Node::shiftLeft(Node *dest, int nshift)
{
	moveChildren(dest, 0, nshift);
	closeHole(0, nshift);
}

/*
 * Adjust the key for this position to match the rightmost
 * key of the child node; corrects the key value following
 * movement of children between nodes.
 */
/// 将第pos个孩子的key设置成它（第pos个孩子）的最后一个孩子的key
void Node::resetKey(int pos)
{
	Node *c = child(pos);
	assert(c != NULL);
	childKey[pos] = c->key();
}

/*!
 * \brief fill an underfull node by borrowing from neighbor
 * \param[in] pos	position of underfull child
 * \return		true if operation succeeded
 *
 * If the underfull child node at position pos has a neighbor
 * that is not underfull, borrow from it to bring this one up
 * to strength.  Return true if the operation succeeded.
 */
/// 从左侧邻居或右侧邻居移动一部分孩子到pos下：具体的移动策略见代码内注释
bool Node::balanceNeighbor(int pos)
{
	assert(!hasleaves());
	Node *left = leftNeighbor(pos);
	Node *right = rightNeighbor(pos);
	Node *middle = child(pos);

	// 左侧邻居的孩子数和右侧邻居的孩子数
	int lc = (left == NULL) ? -1 : left->children();
	int rc = (right == NULL) ? -1 : right->children();

	// 选择孩子多的一方
	Node *donor = (lc >= rc) ? left : right;
	if (donor == NULL)
		return false;

	// 移动策略：多出NKEY一半的部分的一半
	int nmove = donor->excess();
	if (nmove <= 0)
		return false;

	assert(nmove + middle->children() <= NKEY);

	if (donor == left)
	{
		// 将左侧邻居的末尾nmove个孩子移动到本Node开头
		left->shiftRight(middle, nmove);
		resetKey(pos - 1);
	}
	else
	{
		// 将右侧邻居的开头nmove个孩子移动到本Node末尾
		right->shiftLeft(middle, nmove);
		resetKey(pos);
	}

	return true;
}

/*!
 * \brief Insert the specified item in the tree.
 * \param item	the item to be inserted
 * \return	status code
 *
 * As we descend from the root, we split any full nodes encountered
 * along the path to ensure that there will be room at every level
 * to accommodate the insertion.  This eager splitting makes the tree
 * slightly larger than strictly necessary, but simplifies the algorithm,
 * since we don't need to backtrack from the leaf up to the first nonfull
 * node.
 *
 * The return value is zero if the insertion succeeds and
 * an error code if it fails (e.g., because duplicates are
 * not allowed).
 */
/// 在有序树中插入元素，使该树仍然保持有序
int Tree::insert(Meta *item)
{
	Key mkey = item->key();
	Node *n = root, *dad = NULL;// dad初始化为0，因为第一个Node为root
	int cpos, dpos = -1;

	for (;;)
	{
		/// 查找第一个大于等于mkey的元素的下标
		cpos = n->findplace(mkey);

		// 如果找到的节点的孩子已经饱和，则需要将该节点拆开，并且继续查找
		if (n->isfull())
		{
			/// 自顶向下分解所有已经存储满的节点
			Node *brother = n->split(this, dad, dpos);
			if (cpos >= n->children())
			{
				n = brother;
				cpos = n->findplace(mkey);
			}
		}

		// 当找到一个叶子节点时退出循环
		if (n->hasleaves())
			break;
		dad = n;
		dpos = cpos;
		n = dad->child(dpos);
	}

	// 在n中插入元素，形成有序树
	n->insertData(&mkey, item, cpos);
	return 0;
}

/*
 * Return the leaf node containing the first instance of
 * the specified key.
 */
/// 在树中查找key为k的第一个叶子节点
Node *Tree::findLeaf(const Key &k) const
{
	Node *n = root;
	int p = n->findplace(k);

	while (!n->hasleaves() && p != n->children())
	{
		n = n->child(p);
		p = n->findplace(k);
	}

	// 如果p不是最末位置（最末位置说明没有查找到）并且n的key是k，则返回n；否则，返回NULL
	return (p != n->children() && n->getkey(p) == k) ? n : NULL;
}

/*
 * If searching carries us into a new level-1 node below, shift the
 * next level of the descent path over by one, repeating as necessary
 * at higher levels.
 */
/// 计算当前路径指向节点的下一个节点：如果当前节点已经是该Node的最后一个孩子，则下一个位置应该是
/// 这个节点相邻节点的第一个孩子
void Tree::shift_path(vector<pathlink> &path)
{
	int i = path.size();
	bool done = false;

	while (!done)
	{
		assert(i != 0);
		pathlink pl = path[--i];
		pl.pos++;
		done = (pl.pos != pl.n->children());
		if (!done)
		{	// 如果已经超出边界，即p1.pos == p1.n->children()
			pl.pos = 0;
			pl.n = pl.n->peer();
		}
		path[i] = pl;
	}
}

/*!
 * \brief Delete specified item from tree.
 * \param[in] m		the item to be deleted
 * \return		0 on success, -1 otherwise
 */
/// 从Tree中删除指定的节点
int Tree::del(Meta *m)
{
	Key mkey = m->key();

	// 用来记录搜索路径
	vector<pathlink> path;
	Node *dad;
	bool removed = false;

	/*
	 *  Descend to the appropriate leaf, remembering the
	 *  path that we traverse from the root.
	 */
	Node *n = root;
	int pos = n->findplace(mkey);

	// 从root节点开始，一直查找到叶子节点，并且记录搜索路径
	while (!n->hasleaves())
	{
		assert(pos != n->children());
		path.push_back(pathlink(n, pos));
		n = n->child(pos);
		pos = n->findplace(mkey);
	}

	/*
	 * Try to remove the item.  Since there can be multiple
	 * items with the same key, we have to iterate here to
	 * find the right one (if it exists).  If we jump to a new
	 * level-1 node, we have to update the path correspondingly.
	 */
	// 因为可能会存在多个相同key的节点，所以应该找到fid相同的meta再进行删除
	// n是搜索到的第一个key相同的元素
	Node *n0 = n;

	LeafIter li(n, pos);
	while (!removed && mkey == n->getkey(pos))
	{
		// 如果两个节点对应的fid(meta的拥有者)相同，则直接删除该meta即可
		if (m->match(n->leaf(pos)))
		{
			n->remove(pos);
			removed = true;
		}
		else
		{
			li.next();
			n = li.parent();
			pos = li.index();

			// 如果不相等，则说明该节点其父亲节点的最后一个孩子，需要调用shift_path()获取
			// 相邻的下一个节点
			if (n != n0)
			{
				/// 获取li所指向的元素的搜索路径
				shift_path(path);
				n0 = n;
			}
		}
	}

	// 如果没有删除，则出错返回
	if (!removed)
		return -1;

	/// 此时, 指定元素已经被删除, 需要做进一步工作: 如果删除的是最末一个元素, 则其父亲节点的
	/// key也应该改变, 以保证整棵树的有序性
	if (pos == n->children())
	{
		int i = path.size();
		while (i != 0)
		{
			dad = path[--i].n;
			pos = path[i].pos;
			dad->resetKey(pos);
			if (pos != dad->children() - 1)
				break; /* stop if not rightmost child */
		}
	}

	/*
	 * Now go back up the path to see if we can eliminate any
	 * underfull nodes, either by merging them into a neighbor
	 * or by borrowing children from a neighbor that has an excess
	 * of them.  Stop at the first success, since the rearrangement
	 * may invalidate the saved path.
	 */
	// 执行路径中节点元素均衡操作: 同邻居合并, 或者从邻居移动过来节点
	// @note 应该在第一个成功的操作处退出, 因为一旦成功, path将会发生变化
	bool balanced = false;
	dad = NULL;
	while (!path.empty() && !balanced)
	{
		// 获取最后一个路径元素
		pathlink pl = path.back();
		path.pop_back();
		dad = pl.n;
		pos = pl.pos;
		assert(dad->child(pos) == n);

		// 如果平衡, 即不需要进行调整: 节点数 < 最多节点数的一半, 并且可以进行与邻居的合并;
		// 或者成功从左右节点移动一部分孩子到该节点
		// @note 此处已经保证了在成功完成一次负载均衡操作后就退出循环
		balanced = n->isdepleted() && (dad->mergeNeighbor(pos)
				|| dad->balanceNeighbor(pos));
		n = dad;
	}

	/*
	 * If we rearranged children of the root, check whether the
	 * root is left with just one child, in which case we can reduce
	 * the height of the tree.
	 */
	// 如果我们对root上的孩子节点进行了均衡(合并等操作), 使得root只有一个孩子节点时, 我们
	// 应当降低树的高度
	if (balanced && dad == root)
		poproot();

	return 0;
}

/*!
 * \brief Push a new root onto the top of the tree.
 * \param[in] brother	this node  holds the overflow from the old root
 *			and becomes its sibling.
 */
/// 在root之上生成一个新的root，作为该树的root，同时将原root和brother作为新root的孩子存储
void Tree::pushroot(Node *brother)
{
	Node *newroot = new Node(META_ROOT);

	// 清除原root以及brother中的META_ROOT标记
	root->clearflag(META_ROOT);
	brother->clearflag(META_ROOT);

	// 将原root和brother同时加入到新root下，之后改变树的root，指向新生成的root，并增加
	// 树的高度
	Key k = root->key();
	newroot->addChild(&k, root, 0);
	k = brother->key();
	newroot->addChild(&k, brother, 1);
	root = newroot;
	++hgt;
}

/*!
 * \brief get rid of the root node when it has only one child.
 */
/// 这样操作不会留下无名指针? 并且新的root节点并没有将META_ROOT置位...
void Tree::poproot()
{
	// 如果root的META_LEVEL1没有标记并且它只有一个孩子了, 那么我们将root指定为原root的孩子
	if (!root->hasleaves() && root->children() == 1)
	{
		root = root->child(0);
		--hgt;
	}
}

/*
 * Output commands for debugging.
 */

/*!
 * \brief print all metadata belonging to this leaf node
 */
/// 打印当前节点的孩子数和标志位
const string Node::show() const
{
	std::ostringstream n(std::ostringstream::out);
	n << "internal/" << this << "/children/" << children();
	n << "/flags/" << std::setbase(16) << flags();
	return n.str();
}

/// 执行show操作, 如果n不为空
void showNode(MetaNode *n)
{
	if (n != NULL)
		std::cerr << n->show() << '\n';
}

/// 显示当前节点每一个孩子的信息
void Node::showChildren() const
{
	for_each(childNode, childNode + count, showNode);
}

/*!
 * \brief dump out all of the metadata items for debugging
 */
void Tree::printleaves()
{
	for (Node *n = first; n != NULL; n = n->peer())
	{
		showNode(n);
		n->showChildren();
	}
}
