// Binary Search Tree -- pool task
//
// Flat-array representation:
//   index 0 is the null sentinel (no node)
//   real nodes occupy indices 1 .. size-1
//
// Three parallel arrays store the BST:
//   vals[i]   -- key stored at node i
//   lefts[i]  -- index of left child  (0 = no child)
//   rights[i] -- index of right child (0 = no child)
//
// Functions that only read the tree take the arrays by value (copy).
// Insert is performed iteratively in main, modifying the arrays in place.

// Returns true if key is present in the subtree rooted at 'root'.
fn search(vals: [int32; 16], lefts: [int32; 16], rights: [int32; 16],
          root: int32, key: int32) -> bool {
    let cur: int32 = root;
    while cur != 0 {
        if vals[cur] == key { return true; }
        if key < vals[cur] {
            cur = lefts[cur];
        } else {
            cur = rights[cur];
        }
    }
    return false;
}

// In-order traversal: prints keys in ascending order.
fn inorder(vals: [int32; 16], lefts: [int32; 16], rights: [int32; 16],
           node: int32) -> unit {
    if node == 0 { return; }
    inorder(vals, lefts, rights, lefts[node]);
    print(vals[node]);
    inorder(vals, lefts, rights, rights[node]);
}

fn main() -> int32 {
    // Node storage -- index 0 is the null sentinel and is never written.
    let vals:   [int32; 16] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    let lefts:  [int32; 16] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    let rights: [int32; 16] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];

    let size: int32 = 1;   // next free slot; 0 is permanently the null sentinel
    let root: int32 = 0;   // empty tree

    // Keys to insert, stored in an array so we can loop over them.
    let keys: [int32; 7] = [5, 3, 7, 1, 4, 6, 9];
    let i: int32 = 0;

    while i < 7 {
        let key: int32 = keys[i];

        // Allocate a new node at slot 'size'.
        vals[size]   = key;
        lefts[size]  = 0;
        rights[size] = 0;
        let new_node: int32 = size;
        size = size + 1;

        // Insert into the tree.
        if root == 0 {
            root = new_node;
        } else {
            let cur:    int32 = root;
            let placed: bool  = false;
            while !placed {
                if key < vals[cur] {
                    if lefts[cur] == 0 {
                        lefts[cur] = new_node;
                        placed = true;
                    } else {
                        cur = lefts[cur];
                    }
                } else {
                    if rights[cur] == 0 {
                        rights[cur] = new_node;
                        placed = true;
                    } else {
                        cur = rights[cur];
                    }
                }
            }
        }

        i = i + 1;
    }

    // In-order traversal -- expected output: 1 3 4 5 6 7 9 (one per line)
    print(0);   // separator: marks start of traversal output
    inorder(vals, lefts, rights, root);

    // Search -- key 4 is present, key 8 is not.
    let found:     bool = search(vals, lefts, rights, root, 4);
    let not_found: bool = search(vals, lefts, rights, root, 8);

    if found     { print(1); } else { print(0); }   // expected: 1
    if not_found { print(1); } else { print(0); }   // expected: 0

    return 0;
}
