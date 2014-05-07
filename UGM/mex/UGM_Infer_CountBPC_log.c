#include <math.h>
#include "mex.h"
#include "UGM_common.h"

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	/* Variables */
	int n,s,e,e2,n1,n2,neigh,Vind,Vind2,s1,s2,
		nNodes,nEdges,maxState,dims[3],
		iter,maxIter,nNbrs,notConverged,
		*edgeEnds,*nStates,*V,*E,*y;
	
	double *nodePot,*edgePot,*nodeCount,*edgeCount,
			*nodeBel,*edgeBel,*logZ,
			q1,q2,d1,d2,
			*logNodePot,*powEdgePot,
			*msg_i,*msg_o,*tmp_i,*tmp_o,*old_msg_i,*old_msg_o,
			z,convTol,minVal,
			energy1,energy2,entropy1,entropy2;
	
	/* Input */
	nodePot = mxGetPr(prhs[0]);
	edgePot = mxGetPr(prhs[1]);
	nodeCount = mxGetPr(prhs[2]);
	edgeCount = mxGetPr(prhs[3]);
	edgeEnds = (int*)mxGetPr(prhs[4]);
	nStates = (int*)mxGetPr(prhs[5]);
	V = (int*)mxGetPr(prhs[6]);
	E = (int*)mxGetPr(prhs[7]);
	maxIter = ((int*)mxGetPr(prhs[8]))[0];
	convTol = ((double*)mxGetPr(prhs[9]))[0];
	
	if (!mxIsClass(prhs[4],"int32")
	||!mxIsClass(prhs[5],"int32")
	||!mxIsClass(prhs[6],"int32")
	||!mxIsClass(prhs[7],"int32")
	||!mxIsClass(prhs[8],"int32"))
		mexErrMsgTxt("edgeEnds, nStates, V, E, maxIter must be int32");
	
	/* Compute sizes */
	nNodes = mxGetDimensions(prhs[0])[0];
	maxState = mxGetDimensions(prhs[0])[1];
	nEdges = mxGetDimensions(prhs[4])[0];
	
	/* Output */
	plhs[0] = mxCreateDoubleMatrix(nNodes,maxState,mxREAL);
	dims[0] = maxState;
	dims[1] = maxState;
	dims[2] = nEdges;
	plhs[1] = mxCreateNumericArray(3,dims,mxDOUBLE_CLASS,mxREAL);
	plhs[2] = mxCreateDoubleMatrix(1,1,mxREAL);
	nodeBel = mxGetPr(plhs[0]);
	edgeBel = mxGetPr(plhs[1]);
	logZ = mxGetPr(plhs[2]);
	
	/* Local copies of nodePot,edgePot */
	logNodePot = mxCalloc(nNodes*maxState,sizeof(double));
	powEdgePot = mxCalloc(maxState*maxState*nEdges,sizeof(double));
	for (n = 0; n < nNodes; n++)
		for (s = 0; s < nStates[n]; s++)
			logNodePot[n+nNodes*s] = log(nodePot[n+nNodes*s]);
	for (e = 0; e < nEdges; e++)
	{
		n1 = edgeEnds[e]-1;
		n2 = edgeEnds[e+nEdges]-1;
		for (s1 = 0; s1 < nStates[n1]; s1++)
			for (s2 = 0; s2 < nStates[n1]; s2++)
				powEdgePot[s1+maxState*(s2+maxState*e)] = pow(edgePot[s1+maxState*(s2+maxState*e)], 1.0/edgeCount[e]);
	}
	
	/* Loop variables */
	msg_i = mxCalloc(maxState*nEdges*2,sizeof(double));
	msg_o = mxCalloc(maxState*nEdges*2,sizeof(double));
	tmp_i = mxCalloc(maxState*nEdges*2,sizeof(double));
	tmp_o = mxCalloc(maxState*nEdges*2,sizeof(double));
	old_msg_i = mxCalloc(maxState*nEdges*2,sizeof(double));
	old_msg_o = mxCalloc(maxState*nEdges*2,sizeof(double));

	/* Initialize messages */
	for (e = 0; e < nEdges; e++)
	{
		n1 = edgeEnds[e]-1;
		n2 = edgeEnds[e+nEdges]-1;
		for (s = 0; s < nStates[n1]; s++) {
			msg_i[s+maxState*e] = -log(nStates[n1]);
			msg_o[s+maxState*e] = -log(nStates[n1]);
		}
		for (s = 0; s < nStates[n2]; s++) {
			msg_i[s+maxState*(e+nEdges)] = -log(nStates[n2]);
			msg_o[s+maxState*(e+nEdges)] = -log(nStates[n2]);
		}
	}
	
	/* Main loop */
	for (iter = 0; iter < maxIter; iter++)
	{
		/* Temp messages */
		for (e = 0; e < nEdges; e++)
		{
			n1 = edgeEnds[e]-1;
			n2 = edgeEnds[e+nEdges]-1;
			
			/* Incoming */
			for (s1 = 0; s1 < nStates[n1]; s1++) {
				minVal = INFINITY;
				for (s2 = 0; s2 < nStates[n1]; s2++) {
					if (minVal > msg_o[s2+maxState*(e+nEdges)])
						minVal = msg_o[s2+maxState*(e+nEdges)];
				}
				z = 0.0;
				for (s2 = 0; s2 < nStates[n1]; s2++)
					z += powEdgePot[s1+maxState*(s2+maxState*e)] * exp(msg_o[s2+maxState*(e+nEdges)]-minVal);
				tmp_i[s1+maxState*e] = log(z) + minVal;
			}
			for (s2 = 0; s2 < nStates[n1]; s2++) {
				minVal = INFINITY;
				for (s1 = 0; s1 < nStates[n1]; s1++) {
					if (minVal > msg_o[s1+maxState*e])
						minVal = msg_o[s1+maxState*e];
				}
				z = 0.0;
				for (s1 = 0; s1 < nStates[n1]; s1++)
					z += powEdgePot[s1+maxState*(s2+maxState*e)] * exp(msg_o[s1+maxState*e]-minVal);
				tmp_i[s2+maxState*(e+nEdges)] = log(z) + minVal;
			}

			/* Outgoing */
			for (s = 0; s < nStates[n1]; s++)
				tmp_o[s+maxState*e] = logNodePot[n1+nNodes*s];
			for(Vind = V[n1]-1; Vind < V[n1+1]-1; Vind++)
			{
				e2 = E[Vind] - 1;
				if (e != e2)
				{
					if (n1 == edgeEnds[e2]-1) {
						for (s = 0; s < nStates[n1]; s++)
							tmp_o[s+maxState*e] += msg_i[s+maxState*e2];
					}
					else {
						for (s = 0; s < nStates[n1]; s++)
							tmp_o[s+maxState*e] += msg_i[s+maxState*(e2+nEdges)];
					}
				}
			}
			for (s = 0; s < nStates[n2]; s++)
				tmp_o[s+maxState*(e+nEdges)] = logNodePot[n2+nNodes*s];
			for(Vind = V[n2]-1; Vind < V[n2+1]-1; Vind++)
			{
				e2 = E[Vind] - 1;
				if (e != e2)
				{
					if (n2 == edgeEnds[e2]-1) {
						for (s = 0; s < nStates[n2]; s++)
							tmp_o[s+maxState*(e+nEdges)] += msg_i[s+maxState*e2];
					}
					else {
						for (s = 0; s < nStates[n2]; s++)
							tmp_o[s+maxState*(e+nEdges)] += msg_i[s+maxState*(e2+nEdges)];
					}
				}
			}
		}
		
		/* New messages */
		for (e = 0; e < nEdges; e++)
		{
			n1 = edgeEnds[e]-1;
			n2 = edgeEnds[e+nEdges]-1;
			
			/* Exponent variables */
			q1 = (1.0-nodeCount[n1]) / (V[n1+1]-V[n1]);
			q2 = (1.0-nodeCount[n2]) / (V[n2+1]-V[n2]);
			d1 = edgeCount[e] - q1 + 1.0;
			d2 = edgeCount[e] - q2 + 1.0;

			/* Incoming */
			z = 0.0;
			for (s = 0; s < nStates[n1]; s++) {
				msg_i[s+maxState*e] = (tmp_i[s+maxState*e] * (edgeCount[e]/d1))
									+ (tmp_o[s+maxState*e] * ((q1-edgeCount[e])/d1));
				z += exp(msg_i[s+maxState*e]);
			}
			z = log(z);
			for (s = 0; s < nStates[n1]; s++)
				msg_i[s+maxState*e] -= z;
			z = 0.0;
			for (s = 0; s < nStates[n2]; s++) {
				msg_i[s+maxState*(e+nEdges)] = (tmp_i[s+maxState*(e+nEdges)] * (edgeCount[e]/d2))
											 + (tmp_o[s+maxState*(e+nEdges)] * ((q2-edgeCount[e])/d2));
				z += exp(msg_i[s+maxState*(e+nEdges)]);
			}
			z = log(z);
			for (s = 0; s < nStates[n2]; s++)
				msg_i[s+maxState*(e+nEdges)] -= z;
			
			/* Outgoing */
			z = 0.0;
			for (s = 0; s < nStates[n1]; s++) {
				msg_o[s+maxState*e] = (tmp_i[s+maxState*e] * ((q1-1)/d1))
									+ (tmp_o[s+maxState*e] * (1/d1));
				z += exp(msg_o[s+maxState*e]);
			}
			z = log(z);
			for (s = 0; s < nStates[n1]; s++)
				msg_o[s+maxState*e] -= z;
			z = 0.0;
			for (s = 0; s < nStates[n2]; s++) {
				msg_o[s+maxState*(e+nEdges)] = (tmp_i[s+maxState*(e+nEdges)] * ((q2-1)/d2))
											 + (tmp_o[s+maxState*(e+nEdges)] * (1/d2));
				z += exp(msg_o[s+maxState*(e+nEdges)]);
			}
			z = log(z);
			for (s = 0; s < nStates[n2]; s++)
				msg_o[s+maxState*(e+nEdges)] -= z;
		}
	
		/* Check convergence */
		notConverged = 0;
		for (s = 0; s < maxState; s++) {
			for (e = 0; e < nEdges*2; e++) {
				if (absDif(exp(msg_i[s+maxState*e]),exp(old_msg_i[s+maxState*e])) >= convTol)
					notConverged++;
				if (absDif(exp(msg_o[s+maxState*e]),exp(old_msg_o[s+maxState*e])) >= convTol)
					notConverged++;
				old_msg_i[s+maxState*e] = msg_i[s+maxState*e];
				old_msg_o[s+maxState*e] = msg_o[s+maxState*e];
			}
		}
		if (notConverged == 0) {
			iter++;
			break;
		}
	}
	
	if(iter == maxIter)
		printf("CountBP did not converge after %d iterations\n",maxIter);
// 	printf("Stopped after %d iterations\n",iter);
	
	/* Compute nodeBel */
	for (n = 0; n < nNodes; n++)
	{
		/* Init to logNodePot */
		for (s = 0; s < nStates[n]; s++)
			nodeBel[n+nNodes*s] = logNodePot[n+nNodes*s];
		/* Multiply by product of incoming messages */
		for (Vind = V[n]-1; Vind < V[n+1]-1; Vind++)
		{
			e = E[Vind]-1;
			n1 = edgeEnds[e]-1;
			n2 = edgeEnds[e+nEdges]-1;
			if (n == n1) {
				for(s = 0; s < nStates[n]; s++)
					nodeBel[n+nNodes*s] += msg_i[s+maxState*e];
			}
			else {
				for(s = 0; s < nStates[n]; s++)
					nodeBel[n+nNodes*s] += msg_i[s+maxState*(e+nEdges)];
			}
		}
		/* Exponentiate and normalize */
		z = 0.0;
		for(s = 0; s < nStates[n]; s++) {
			nodeBel[n+nNodes*s] = exp(nodeBel[n+nNodes*s]);
			z += nodeBel[n+nNodes*s];
		}
		for(s = 0; s < nStates[n]; s++)
			nodeBel[n+nNodes*s] /= z;
	}
	
	/* Compute edgeBel */
	for (e = 0; e < nEdges; e++)
	{
		n1 = edgeEnds[e]-1;
		n2 = edgeEnds[e+nEdges]-1;
		/* Multiply edgePot by cross-product of outgoing messages */
		z = 0.0;
		for (s1 = 0; s1 < nStates[n1]; s1++) {
			for (s2 = 0; s2 < nStates[n2]; s2++) {
				edgeBel[s1+maxState*(s2+maxState*e)] = edgePot[s1+maxState*(s2+maxState*e)]
													 * exp(msg_o[s1+maxState*e] + msg_o[s2+maxState*(e+nEdges)]);
				z += edgeBel[s1+maxState*(s2+maxState*e)];
			}
		}
		/* Normalize */
		for (s1 = 0; s1 < nStates[n1]; s1++) {
			for (s2 = 0; s2 < nStates[n2]; s2++)
				edgeBel[s1+maxState*(s2+maxState*e)] /= z;
		}
	}
	
	/* Compute Bethe Free Energy */
	energy1 = 0;
	energy2 = 0;
	entropy1 = 0;
	entropy2 = 0;
	for (n = 0; n < nNodes; n++)
	{
		nNbrs = V[n+1]-V[n];
		for (s = 0; s < nStates[n]; s++) {
			if (nodeBel[n+nNodes*s] > 1e-10)
				entropy1 += (nNbrs-1)*nodeBel[n+nNodes*s]*log(nodeBel[n+nNodes*s]);
			energy1 -= nodeBel[n+nNodes*s] * logNodePot[n+nNodes*s];
		}
	}
	for (e = 0; e < nEdges; e++)
	{
		n1 = edgeEnds[e]-1;
		n2 = edgeEnds[e+nEdges]-1;
		for (s1 = 0; s1 < nStates[n1]; s1++) {
			for (s2 = 0; s2 < nStates[n2]; s2++) {
				if (edgeBel[s1+maxState*(s2+maxState*e)] > 1e-10)
					entropy2 -= edgeBel[s1+maxState*(s2+maxState*e)] * log(edgeBel[s1+maxState*(s2+maxState*e)]);
				energy2 -= edgeBel[s1+maxState*(s2+maxState*e)] * log(edgePot[s1+maxState*(s2+maxState*e)]);
			}
		}
	}
	logZ[0] = (entropy1 + entropy2) - (energy1 + energy2);
	
	/* Free memory */
	mxFree(logNodePot);
	mxFree(powEdgePot);
	mxFree(msg_i);
	mxFree(msg_o);
	mxFree(tmp_i);
	mxFree(tmp_o);
	mxFree(old_msg_i);
	mxFree(old_msg_o);
}