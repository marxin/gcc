#include <sstream>
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>

#include <iostream>
#include <list>
#include <stack>

#define NIL -1

using namespace std;

// A class that represents an directed graph
class Graph
{
    int V;    // No. of vertices
    list<int> *adj;    // A dynamic array of adjacency lists
 
    // A Recursive DFS based function used by SCC()
    void SCCUtil(int u, int disc[], int low[],
                 stack<int> *st, bool stackMember[]);
public:
    Graph(int V);   // Constructor
    void addEdge(int v, int w);   // function to add an edge to graph
    void SCC();    // prints strongly connected components

    vector<vector<int>> components;
};
 
Graph::Graph(int V)
{
    this->V = V;
    adj = new list<int>[V];
}
 
void Graph::addEdge(int v, int w)
{
    adj[v].push_back(w);
}
 
// A recursive function that finds and prints strongly connected
// components using DFS traversal
// u --> The vertex to be visited next
// disc[] --> Stores discovery times of visited vertices
// low[] -- >> earliest visited vertex (the vertex with minimum
//             discovery time) that can be reached from subtree
//             rooted with current vertex
// *st -- >> To store all the connected ancestors (could be part
//           of SCC)
// stackMember[] --> bit/index array for faster check whether
//                  a node is in stack
void Graph::SCCUtil(int u, int disc[], int low[], stack<int> *st,
                    bool stackMember[])
{
    // A static variable is used for simplicity, we can avoid use
    // of static variable by passing a pointer.
    static int time = 0;
 
    // Initialize discovery time and low value
    disc[u] = low[u] = ++time;
    st->push(u);
    stackMember[u] = true;
 
    // Go through all vertices adjacent to this
    list<int>::iterator i;
    for (i = adj[u].begin(); i != adj[u].end(); ++i)
    {
        int v = *i;  // v is current adjacent of 'u'
 
        // If v is not visited yet, then recur for it
        if (disc[v] == -1)
        {
            SCCUtil(v, disc, low, st, stackMember);
 
            // Check if the subtree rooted with 'v' has a
            // connection to one of the ancestors of 'u'
            // Case 1 (per above discussion on Disc and Low value)
            low[u]  = min(low[u], low[v]);
        }
 
        // Update low value of 'u' only of 'v' is still in stack
        // (i.e. it's a back edge, not cross edge).
        // Case 2 (per above discussion on Disc and Low value)
        else if (stackMember[v] == true)
            low[u]  = min(low[u], disc[v]);
    }
 
    // head node found, pop the stack and print an SCC
    int w = 0;  // To store stack extracted vertices
    if (low[u] == disc[u])
    {
	vector<int> component;
        while (st->top() != u)
        {
            w = (int) st->top();
	    component.push_back (w);
            stackMember[w] = false;
            st->pop();
        }
        w = (int) st->top();
	component.push_back (w);
	components.push_back (component);
        stackMember[w] = false;
        st->pop();
    }
}
 
// The function to do DFS traversal. It uses SCCUtil()
void Graph::SCC()
{
    int *disc = new int[V];
    int *low = new int[V];
    bool *stackMember = new bool[V];
    stack<int> *st = new stack<int>();
 
    // Initialize disc and low, and stackMember arrays
    for (int i = 0; i < V; i++)
    {
        disc[i] = NIL;
        low[i] = NIL;
        stackMember[i] = false;
    }
 
    // Call the recursive helper function to find strongly
    // connected components in DFS tree with vertex 'i'
    for (int i = 0; i < V; i++)
        if (disc[i] == NIL)
            SCCUtil(i, disc, low, st, stackMember);
}

using namespace std;

struct function_entry
{
  function_entry (string name, unsigned lineno_start, unsigned id):
    m_name (name), m_lineno_start (lineno_start), m_id (id),
    m_callees ()
  {}

  unsigned get_loc ()
  {
    return m_lineno_end - m_lineno_start;
  }

  string m_name;
  unsigned m_lineno_start;
  unsigned m_lineno_end;
  unsigned m_id;
  vector<unsigned> m_callees;
};

map<string, unsigned> fn_to_index_map; 
vector<function_entry *> functions;
vector<string> lines;

static unsigned
get_id_for_fname (string fname)
{  
  map<string, unsigned>::iterator it = fn_to_index_map.find (fname);
  if (it != fn_to_index_map.end ())
    return it->second;
  else
    {
      unsigned id = fn_to_index_map.size ();
      fn_to_index_map[fname] = id;
      return id;
    }
}

struct function_component
{
  function_component (vector<int> function_ids):
    m_function_ids (function_ids)
  {}

  void print()
  {
    for (unsigned i = 0; i < m_function_ids.size (); i++)
      printf ("%s ", functions[m_function_ids[i]]->m_name.c_str ());
    printf ("\n");
  }

  unsigned get_total_loc ()
    {
      unsigned loc = 0;
      for (unsigned i = 0; i < m_function_ids.size (); i++)
	loc += functions[m_function_ids[i]]->get_loc ();
      return loc;
    }

  void write (ofstream &s)
    {
      sort (m_function_ids.begin (), m_function_ids.end ());
      for (unsigned i = 0; i < m_function_ids.size (); i++)
	{
	  function_entry *f = functions[m_function_ids[i]];
	  for (unsigned j = f->m_lineno_start; j <= f->m_lineno_end; j++)
	    s << lines[j] << endl;
	  s << endl;
	}
    }

  vector<int> m_function_ids;
};

bool component_size_cmp (function_component *a, function_component *b)
{
  return a->get_total_loc () < b->get_total_loc ();
}

vector<function_component *> components;

int
main (int argc, char **argv)
{
  if (argc != 2)
    return -1;

  string type (argv[1]);
  string folder = "/dev/shm/objdir/gcc/";

  ifstream infile(folder + type + "-match.c");
  ofstream header(folder + type + "-match-header.c");
  ofstream footer(folder + type + "-match-part-footer.c");
  footer << "#include \"" << type << "-match-header.c\"" << endl;

  string line;
  unsigned lineno = 0;
  bool in_split = false;
  bool header_done = false;

  while (getline(infile, line))
    {
      string fnbegin ("// split-fn-begin:");
      string fnend ("// split-fn-end");
      string call ("// call-fn:");
      lines.push_back (line);

      if (line.find(fnbegin) != string::npos)
	{
	  in_split = true;
	  header_done = true;
	  string fname = line.substr (fnbegin.length ());
	  functions.push_back (new function_entry (fname, lineno,
					     get_id_for_fname (fname)));
	}
      else if (line.find (fnend) != string::npos)
	{
	  functions.back ()->m_lineno_end = lineno;
	  in_split = false;
	}
      else if (line.find (call) != string::npos)
	{
	  string fname = line.substr (call.length ());
	  functions.back ()->m_callees.push_back (get_id_for_fname (fname));
	}
      else if (!in_split && !line.empty ())
	{
	  if (header_done)
	    footer << line << endl;
	  else
	    header << line << endl;
	}

      lineno++;
    }

  /*
  for (unsigned i = 0; i < functions.size (); i++)
    {
      function_entry *f = functions[i];
      fprintf (stderr, "%d %s: %d\n",
	       f->m_lineno_end - f->m_lineno_start,
	       f->m_name.c_str (), f->m_callees.size ());
    }
    */

  /* Create graph and compute SCC.  */
  Graph g (functions.size ());
  for (unsigned i = 0; i < functions.size (); i++)
    {
      function_entry *f = functions[i];
      for (unsigned j = 0; j < f->m_callees.size (); j++)
	{
	  g.addEdge (f->m_id, f->m_callees[j]);
	  g.addEdge (f->m_callees[j], f->m_id);
	}
    }
  g.SCC ();

  for (unsigned i = 0; i < g.components.size (); i++)
    components.push_back (new function_component(g.components[i]));

  sort (components.begin (), components.end (), component_size_cmp);

  unsigned total_loc = 0;
  for (unsigned i = 0; i < components.size (); i++)
    {
//      components[i]->print();
      total_loc += components[i]->get_total_loc ();
    }

  printf ("Total # of functions: %ld, total LOC: %u\n", functions.size (),
	  total_loc);

  vector<vector<function_component *>> groups;

  unsigned parts = 4;
  unsigned component_size = total_loc / parts;
  if (components.back ()->get_total_loc () > component_size)
    component_size = components.back ()->get_total_loc ();

  for (unsigned i = 0; i < parts; i++)
    {
      unsigned space = component_size;
      vector<function_component *> group;
      for (int j = components.size () - 1; j >= 0; j--)
	{
	  function_component *c = components[j];
	  unsigned loc = c->get_total_loc ();
	  if (space >= loc || (i == parts - 1))
	    {
//	      fprintf (stderr, "adding %d to group %d\n", loc, i);
	      components.erase (components.begin () + j);
	      space -= loc;
	      group.push_back (c);
	    }
	}

      groups.push_back (group);
    }

  for (unsigned i = 0; i < groups.size (); i++)
    {
      string name = folder + type + "-match-part-" + std::to_string (i) + ".c";
      ofstream s (name);
      s << "#include \"" << type << "-match-header.c\"" << endl;

      unsigned loc = 0;
      for (unsigned j = 0; j < groups[i].size (); j++)
	{
	  function_component *c = groups[i][j];
	  loc += c->get_total_loc ();
	  c->write (s);
	}
      s.close ();

      fprintf (stderr, "written %d LOC functions to %s\n", loc, name.c_str ());
    }

  header.close ();
  footer.close ();

  return 0;
}
