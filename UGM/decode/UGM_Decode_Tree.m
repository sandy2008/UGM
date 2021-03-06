function  [nodeLabels,nodeBel,messages] = UGM_Decode_Tree(nodePot, edgePot, edgeStruct)
% INPUT
% nodePot(node,class)
% edgePot(class,class,edge) where e is referenced by V,E (must be the same
% between feature engine and inference engine)
%
% OUTPUT
% nodeLabels - decoded labels
% nodeBel(node,class) - max-marginals
% messages(state,edge) - messages from BP
%
% Assumes no ties


[nNodes,maxState] = size(nodePot);
nEdges = size(edgePot,3);
edgeEnds = edgeStruct.edgeEnds;
nStates = edgeStruct.nStates;
V = edgeStruct.V;
E = edgeStruct.E;

% Compute Messages
maximize = 1;
messages = UGM_TreeBP(nodePot,edgePot,edgeStruct,maximize);

% Compute nodeBel
for n = 1:nNodes
   nodeBel(n,1:nStates(n)) = nodePot(n,1:nStates(n));

   edges = UGM_getEdges(n,edgeStruct);
   for e = edges
      if n == edgeEnds(e,2)
         nodeBel(n,1:nStates(n)) = nodeBel(n,1:nStates(n)).*messages(1:nStates(n),e)';
      else
         nodeBel(n,1:nStates(n)) = nodeBel(n,1:nStates(n)).*messages(1:nStates(n),e+nEdges)';
      end

   end
   nodeBel(n,1:nStates(n)) = nodeBel(n,1:nStates(n))./sum(nodeBel(n,1:nStates(n)));
end

[pot nodeLabels] = max(nodeBel,[],2);
end
