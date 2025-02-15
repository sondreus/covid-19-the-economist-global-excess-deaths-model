// tree.hpp

#ifndef __TREE_HPP_INCLUDED__
#define __TREE_HPP_INCLUDED__


#include "node.hpp"
#include "external_rcpp.hpp"


class GBTREE
{
    
    //private:
public:
    
    node* root;
    GBTREE* next_tree;
    
    GBTREE();

    node* getRoot();
    void train(Tvec<double> &g, Tvec<double> &h, Tmat<double> &X, Tmat<double> &cir_sim,
               bool greedy_complexities, double learning_rate, int maxDepth=1);
    double predict_obs(Tvec<double> &x);
    Tvec<double> predict_data(Tmat<double> &X);
    double getTreeScore();
    double getConditionalOptimism();
    double getFeatureMapOptimism();
    double getTreeOptimism(); // sum of the conditional and feature map optimism
    int getNumLeaves();
    void print_tree(int type);
    void serialize(GBTREE* tptr, std::ofstream& f);
    bool deSerialize(GBTREE* tptr,  std::ifstream& f, int& lineNum, int &numRead);
    void importance(Tvec<double> &importance_vector, double learning_rate);
    
};


// METHODS

void GBTREE::serialize(GBTREE *tptr, std::ofstream& f)
{
    
    int MARKER = -1;
    // If current tree is NULL, store marker
    if(tptr == NULL)
    {
        f << MARKER;
        return;
    }
    
    // Else, store current tree, recur on next tree
    tptr->root->serialize(tptr->root, f);
    serialize(tptr->next_tree, f);
}

bool GBTREE::deSerialize(GBTREE *tptr, std::ifstream& f, int& lineNum, int &numRead)
{
    // If not MARKER, deserialize root node (unincremented lineNum)
    tptr->root = new node;
    const bool success = tptr->root->deSerialize(tptr->root, f, lineNum, numRead); // lineNum passed by reference and incremented
    if (!success) {
        tptr = NULL;
        return false;
    }
    numRead++;
    GBTREE* new_tree = new GBTREE;
    bool new_tree_exist = deSerialize(new_tree, f, lineNum, numRead);
    if(new_tree_exist)
    {
        tptr->next_tree = new_tree;
    }else{
        tptr->next_tree = NULL;
    }
    
    return true;
}

    
GBTREE::GBTREE(){
    this->root = NULL;
    this->next_tree = NULL;
}

node* GBTREE::getRoot(){
    return this->root;
}


void GBTREE::train(Tvec<double> &g, Tvec<double> &h, Tmat<double> &X, Tmat<double> &cir_sim,
                   bool greedy_complexities, double learning_rate, int maxDepth)
{
    // Check if root exists 
    // Else create root
    int n = g.size();
    
    if(root == NULL){
        // Calculate information
        double G=0, H=0, G2=0, H2=0, gxh=0;
        for(int i=0; i<n; i++){
            G += g[i]; H+=h[i];
            G2 += g[i]*g[i]; H2 += h[i]*h[i];
            gxh += g[i]*h[i];
        }
        double local_optimism = (G2 - 2.0*gxh*(G/H) + G*G*H2/(H*H)) / (H*n);
        
        node* root_ptr = new node;
        root_ptr->createLeaf(-G/H, -G*G/(2*H*n), local_optimism, local_optimism, n, n, n);
        root = root_ptr;
        //root = root->createLeaf(-G/H, -G*G/(2*H*n), local_optimism, local_optimism, n, n, n);
    }
    
    // Root-node indices
    Tvec<int> ind(n);
    std::iota(ind.data(), ind.data()+ind.size(), 0);
    
    root->split_node(g, h, ind, X, cir_sim, root, n, 0.0, greedy_complexities, learning_rate, 0, maxDepth);
    
}

double GBTREE::predict_obs(Tvec<double> &x){
    
    node* current = this->root;
    
    if(current == NULL){
        return 0;
    }
    
    
    while(current != NULL){
        if(current->left == NULL && current ->right == NULL){
            return current->node_prediction;
        }
        else{
            
            if(x[current->split_feature] <= current->split_value){
                current = current->left;
            }else{
                current = current->right;
            }
        }
    }
    return 0;
}

Tvec<double> GBTREE::predict_data(Tmat<double> &X){
    
    int n = X.rows();
    Tvec<double> res(n), x(n);
    
    for(int i=0; i<n; i++){
        x = X.row(i);
        res[i] = predict_obs(x);
    }
    return res;
    
}

double GBTREE::getTreeScore(){
    // Recurse tree and sum leaf scores
    double treeScore = 0;
    
    node* current = this->root;
    node* pre;
    
    if(current == NULL){
        return 0;
    }
    
    while (current != NULL) { 
        
        if (current->left == NULL) {
            //std::cout <<  current->node_prediction << std::endl; 
            treeScore += current->node_tr_loss;
            current = current->right;
        } 
        else { 
            
            /* Find the inorder predecessor of current */
            pre = current->left; 
            while (pre->right != NULL && pre->right != current) 
                pre = pre->right; 
            
            /* Make current as right child of its inorder 
             predecessor */
            if (pre->right == NULL) { 
                pre->right = current; 
                current = current->left; 
            } 
            
            /* Revert the changes made in if part to restore 
             the original tree i.e., fix the right child 
             of predecssor */
            else { 
                pre->right = NULL; 
                current = current->right; 
            } /* End of if condition pre->right == NULL */
        } /* End of if condition current->left == NULL*/
    } /* End of while */
            
            return treeScore;
}

double GBTREE::getConditionalOptimism(){
    // Recurse tree and sum conditional optimism in leaves
    double conditional_opt_leaves = 0;
    
    node* current = this->root;
    node* pre;
    
    if(current == NULL){
        return 0;
    }
    
    while (current != NULL) { 
        
        if (current->left == NULL) { 
            //std::cout <<  current->node_prediction << std::endl; 
            conditional_opt_leaves += current->local_optimism * current->prob_node;
            current = current->right; 
        } 
        else { 
            
            /* Find the inorder predecessor of current */
            pre = current->left; 
            while (pre->right != NULL && pre->right != current) 
                pre = pre->right; 
            
            /* Make current as right child of its inorder 
             predecessor */
            if (pre->right == NULL) { 
                pre->right = current; 
                current = current->left; 
            } 
            
            /* Revert the changes made in if part to restore 
             the original tree i.e., fix the right child 
             of predecssor */
            else { 
                pre->right = NULL; 
                current = current->right; 
            } /* End of if condition pre->right == NULL */
        } /* End of if condition current->left == NULL*/
    } /* End of while */
            
            return conditional_opt_leaves;
}


double GBTREE::getTreeOptimism(){
    
    // Recurse tree and sum p_split_CRt in leaf-nodes
    double tree_optimism = 0.0;
    
    node* current = this->root;
    node* pre;
    
    if(current == NULL){
        return 0;
    }
    
    while (current != NULL) { 
        
        if (current->left == NULL) { 
            //std::cout <<  current->node_prediction << std::endl; 
            //conditional_opt_leaves += current->local_optimism * current->prob_node;
            current = current->right; 
        } 
        else { 
            
            /* Find the inorder predecessor of current */
            pre = current->left; 
            while (pre->right != NULL && pre->right != current) 
                pre = pre->right; 
            
            /* Make current as right child of its inorder 
             predecessor */
            if (pre->right == NULL) { 
                pre->right = current; 
                current = current->left; 
            } 
            
            /* Revert the changes made in if part to restore 
             the original tree i.e., fix the right child 
             of predecssor */
            else { 
                pre->right = NULL; 
                tree_optimism += current->CRt; // current->split_point_optimism;
                current = current->right; 
            } /* End of if condition pre->right == NULL */
        } /* End of if condition current->left == NULL*/
    } /* End of while */
            
    return tree_optimism;
    
}

void GBTREE::importance(Tvec<double> &importance_vector, double learning_rate){
    
    // Recurse tree and sum importance (reduction in generalization loss)
    int importance_feature = 0;
    
    node* current = this->root;
    node* pre;
    
    if(current == NULL){
        return;
    }
    
    while (current != NULL) { 
        
        if (current->left == NULL) { 
            //std::cout <<  current->node_prediction << std::endl; 
            //conditional_opt_leaves += current->local_optimism * current->prob_node;
            current = current->right; 
        } 
        else { 
            
            /* Find the inorder predecessor of current */
            pre = current->left; 
            while (pre->right != NULL && pre->right != current) 
                pre = pre->right; 
            
            /* Make current as right child of its inorder 
             predecessor */
            if (pre->right == NULL) { 
                pre->right = current; 
                current = current->left; 
            } 
            
            /* Revert the changes made in if part to restore 
             the original tree i.e., fix the right child 
             of predecssor */
            else { 
                pre->right = NULL; 
                importance_feature = current->split_feature;
                importance_vector[importance_feature] += current->expected_reduction(learning_rate);
                current = current->right; 
            } /* End of if condition pre->right == NULL */
        } /* End of if condition current->left == NULL*/
    } /* End of while */
            
    return;
    
}

int GBTREE::getNumLeaves(){
    int numLeaves = 0;
    node* current = this->root;
    node* pre;
    
    if(current == NULL){
        return 0;
    }
    
    while (current != NULL) { 
        
        if (current->left == NULL) { 
            //std::cout <<  current->node_prediction << std::endl; 
            numLeaves += 1;
            current = current->right; 
        } 
        else { 
            
            /* Find the inorder predecessor of current */
            pre = current->left; 
            while (pre->right != NULL && pre->right != current) 
                pre = pre->right; 
            
            /* Make current as right child of its inorder 
             predecessor */
            if (pre->right == NULL) { 
                pre->right = current; 
                current = current->left; 
            } 
            
            /* Revert the changes made in if part to restore 
             the original tree i.e., fix the right child 
             of predecssor */
            else { 
                pre->right = NULL; 
                current = current->right; 
            } /* End of if condition pre->right == NULL */
        } /* End of if condition current->left == NULL*/
    } /* End of while */
            
    return numLeaves;
}



#endif
