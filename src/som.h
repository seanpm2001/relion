/***************************************************************************
 *
 * Author: "Dari Kimanius"
 * MRC Laboratory of Molecular Biology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This complete copyright notice must be included in any revised version of the
 * source code. Additional authorship citations may be added, but existing
 * author citations must be preserved.
 ***************************************************************************/

#ifndef SOM_H
#define SOM_H

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "src/parallel.h"

class SomGraph {

private:

	/**
	 * Class for graph nodes
	 */
	struct Node {
		float error = 0.;
	};

	/**
	 * Class for graph edges
	 */
	class Edge {
	public:
		float age = 0.;
		unsigned n1, n2;
		Edge (unsigned node1, unsigned node2):
				n1(node1), n2(node2)
		{};
	};

	std::unordered_map<unsigned, Node> _nodes;
	std::vector<Edge> _edges;

	pthread_mutex_t mutex;

	void _remove_node(unsigned node) {
		if (_nodes.find(node) == _nodes.end())
			throw std::runtime_error("node missing");

		for (unsigned i = 0; i < _edges.size(); i++)
			if (_edges[i].n1 == node || _edges[i].n2 == node)
				_edges.erase(_edges.begin()+i);

		_nodes.erase(node);
	}

public:

	/**
	 * Add an edge-less node to the graph.
	 */
	unsigned add_node() {
		Lock ml(&mutex);
		for (unsigned i = 0; i < _nodes.size() + 1; i++)
			if (_nodes.find(i) == _nodes.end()) { // If index not found
				_nodes.emplace(i, Node{});
				return i;
			}
		throw std::runtime_error("failed to add node");
	}

	/**
	 * Add a connection between node1 and node2.
	 */
	void add_edge(unsigned node1, unsigned node2) {
		if (node1 == node2)
			throw std::runtime_error("cannot add edge to same node");

		if (_nodes.find(node1) == _nodes.end() || _nodes.find(node2) == _nodes.end())
			throw std::runtime_error("node missing");

		Lock ml(&mutex);

		for (unsigned i = 0; i < _edges.size(); i++) {
			if (_edges[i].n1 == node1 && _edges[i].n2 == node2 ||
			    _edges[i].n1 == node2 && _edges[i].n2 == node1)
				return;
		}

		_edges.emplace_back(node1, node2);
	}

	/**
	 * Remove node.
	 */
	void remove_node(unsigned node) {
		Lock ml(&mutex);
		_remove_node(node);
	}

	/**
	 * Remove edge.
	 */
	void remove_edge(unsigned node1, unsigned node2) {
		Lock ml(&mutex);
		for (unsigned i = 0; i < _edges.size(); i++) {
			if (_edges[i].n1 == node1 && _edges[i].n2 == node2 ||
			    _edges[i].n1 == node2 && _edges[i].n2 == node1) {
				_edges.erase(_edges.begin()+i);
				return;
			}
		}
		throw std::runtime_error("edge not found");
	}

	/**
	 * Get neighbours of given node.
	 */
	std::vector<unsigned> get_neighbours(unsigned node) {
		Lock ml(&mutex);
		std::vector<unsigned> neighbours;
		for (unsigned i = 0; i < _edges.size(); i++) {
			if (_edges[i].n1 == node)
				neighbours.push_back(_edges[i].n2);
			if (_edges[i].n2 == node)
				neighbours.push_back(_edges[i].n1);
		}
		return neighbours;
	}

	/**
	 * Remove all edges older than max_age
	 */
	void purge_old_edges(float max_age) {
		Lock ml(&mutex);
		for (unsigned i = 0; i < _edges.size(); i++) {
			if (_edges[i].age > max_age)
				_edges.erase(_edges.begin()+i);
		}
	}

	/**
	 * Remove all edge-less nodes.
	 * Return them as a list.
	 */
	std::vector<unsigned> purge_orphans() {
		Lock ml(&mutex);
		std::vector<unsigned> orphans;
		for(std::unordered_map<unsigned, Node>::iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
			bool is_orphan = true;
			for (unsigned i = 0; i < _edges.size(); i++)
				if (_edges[i].n1 == n->first || _edges[i].n2 == n->first) {
					is_orphan = false;
					break;
				}
			if (is_orphan)
				orphans.push_back(n->first);
		}

		for (unsigned i = 0; i < orphans.size(); i++)
			_remove_node(orphans[i]);

		return orphans;
	}

	/**
	 * Getters and setters.
	 */

	float get_edge_age(unsigned node1, unsigned node2) {
		Lock ml(&mutex);
		for (unsigned i = 0; i < _edges.size(); i++) {
			if (_edges[i].n1 == node1 && _edges[i].n2 == node2 ||
			    _edges[i].n1 == node2 && _edges[i].n2 == node1)
				return _edges[i].age;
		}
		throw std::runtime_error("edge not found");
	}

	void set_edge_age(unsigned node1, unsigned node2, float age) {
		Lock ml(&mutex);
		for (unsigned i = 0; i < _edges.size(); i++)
			if (_edges[i].n1 == node1 && _edges[i].n2 == node2 ||
			    _edges[i].n1 == node2 && _edges[i].n2 == node1) {
				_edges[i].age = age;
				return;
			}
		throw std::runtime_error("edge not found");
	}

	float get_node_error(unsigned node) {
		Lock ml(&mutex);
		return _nodes[node].error;
	}

	void set_node_error(unsigned node, float error) {
		Lock ml(&mutex);
		_nodes[node].error = error;
	}

	void increment_age() {
		Lock ml(&mutex);
		for (unsigned i = 0; i < _edges.size(); i++)
			_edges[i].age ++;
	}

	unsigned get_node_count() const {
		return _nodes.size();
	}

	unsigned get_edge_count() const {
		return _edges.size();
	}

	void reset_errors() {
		Lock ml(&mutex);
		for(std::unordered_map<unsigned, Node>::iterator n = _nodes.begin(); n != _nodes.end(); ++n)
			n->second.error = 0.;
	}

	unsigned find_wpu() {
		Lock ml(&mutex);
		int wpu = -1;
		XFLOAT min_e = 0;
		for(std::unordered_map<unsigned, Node>::iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
			float e = n->second.error;
			if ((0 <= e && e < min_e) || wpu == -1)
			{
				wpu = n->first;
				min_e = e;
			}
		}
		return wpu;
	}
};

#endif
