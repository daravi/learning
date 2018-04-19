'''
training data: input + output (lable)

sigmoid function 1/(1+e^-z) also known as logistic function

logistic regression:
	- reducing input data to a dichotomous output (using linearization and sigmoid projection)
	- the output is a probability value for successful classification
	- can be thought of as a single layer neural network (good starting point for the subject of NNs)
	- loss function defined as: -log(p(x|y)) = -[y * logy^ + (1-y) * lo(1-y^)] (to ensure convexity)
		(negative log-likelihood cost for logistic regression)
forward propogation (sometimes aka the classifier): predicted output from input
Z = np.dot(w.T, X) + b # b is the bias (aka the intercept) and is broadcasted
Y_pred = 1/(1+np.exp(-Z))
backward propogation: output derivatives wrt inputs (the gradient)
dZ = Y_pred - Y_lable # dVarname means derivative of cost function wrt varname
dw = 1/m * X*dZ.T
db = 1/m * np.sum(dZ)
gradiant decent: using a convex cost function (some of losses over all training data), we can calculate gradient and move in that direction with a specific learning rate alpha (e.g. using logistic regression as the cost function)
w = w - alpha * dw
b = b - alpha * db
vectorization: in the order of 300x faster in numpy (using cpu)
broadcasting: applying a constant to a shaped object python "broadcasts" the constant to each element

using jupyter shift + entre runs the code and moves to next block.

support vector machines?

You can think of softmax as a normalizing function used when your algorithm needs to classify two or more classes. 

Overfitting: when by increasing model's fit to training data the fit to testing data is reduced (model becomes less general than the data's entropy)
	reduce overfitting, for example by using regularization

Question: why does linear regression work with image classifiers? Why are the image vectors close enough to be classified into the same category? It seems to me that if for example the cats are at different corners of the image you would get a more different vectors than if a cat and a tree where in the same corner just because they have the same number of colored pixels in the same spacial region. (and in the same region in their vectorized representation)

Check this assumption ^^^ by trying out the model with custom images

Q) Try logistic regression for cat/ non-cat using a different loss function (for example norm-2), will it really be non-convex?


WEEK 3

If the learning rate is too large we may "overshoot" the optimal value. Similarly, if it is too small we will need too many iterations to converge to the best values.

hidden layers: layers for which the values are unkown (training data = input layer data + output layer data)

activation: value of the previous layer which is fed into the next layer

traditionally we do not count the input layer

a[0] is an alias for the input vector x (where features are the nodes)

Activation function:
	- sigmoid function is an example of an activation function ( Q) why between 0 and 1?)
	- tanh better than sigmoid since it "centers" the data for the next layer (it's a shifted and scaled version)
	  Q) why does centering the data matter?

	- sigmoid suitable for final layer in a binary classification (since we want to get probability between 0 and 1)
	  Q) why not other functions that output 0-1?

	- tanh not suitable since at large z values the gradient is close to zero and it takes long to converge
	  Q) why not standardize the data? Also we know that the output of one layer is between -1 and 1 so not large

	- use ReLU (Rectified Linear Unit) --/ function for hidden layer activation function since slope is always high
	  Q) but ReLU's slope is zero for z < 0 so it will not diverge at all (lecture claims it is ok -> FQ) why ok?)
	  A) because in practice enough of the hidden units will have slope > 0 to make learning fast
	  	- leaky ReLU (max(0.01*z, z) to avoid zero slope ( FQ) why avoid it if not an issue?)
	  	  Q) why not use y = abs(x)? https://www.quora.com/What-are-disadvantages-of-abs-x-as-an-activation-function-in-neural-networks  ??...
	  	   - you can also learn the "leakiness" of the leaky ReLU however it's not very common to do so.

	- activation function has to be non-linear
	  Q) why? A) because if linear then the neural net will only be able to produce an output as a linear function of the inputs (because composition of linear transformations is a linear transformation). We might use a linear function at the output layer for non-probability (real value) outputs (also in compression we might use a linear A.F.)



Subgradient: a modified version of the gradient function to eliminate undefined regions

Better to initialize parameters randomly than initialize to zeroes. Why?
A) because the hidden layers would otherwise all compute the same thing (show this)

Better to initialize as array of small numbers (in the order of 0.01) since the sigmoid creates low gradiant values at large z
Q) but z depends on both weights and x not just weights. (possible answer: only true for the first few layers)

rank 1 array: (m,)


Assignment:
Q) why do we need the parameters in the cost function

WEEK 4


Q) what about padding zero activated nodes to different layers to vectorize forward and backward propogation computations over L layers?


Circuit theory: it is possible to compute a function with small and deep neural network, where as it would require an exponentially large and shallow neural network to implment.
depth of a network with n computation using a computation tree is O(log(n)) (O for Order)
but using a single layer we have to exhaust all 2^n possible configurations to be able to calculate the result of n inputs: O(2^n)




Idea: Using control theory in hyper parameter tuning?

Q) why do we use logistic loss function?

Q) are cross-entropy cost and logistic loss (log loss?) the same?

do log and logistic and logic have the same root?
'''

dAL = - (np.divide(Y, AL) - np.divide(1 - Y, 1 - AL)) # derivative of cost with respect to AL

train_x_flatten = train_x_orig.reshape(train_x_orig.shape[0], -1).T   # The "-1" makes reshape flatten the remaining dimensions


'''
Course 2:


Week 1:

Train set - Dev set (aka holdout cross-validation set) - Test set
If we do not need an unbiased estimate of the model performance we don't need the test set

high bias: train set erro >> baysian erro -> underfitting (are we doing a good job fitting the training data, i.e. learning the function?)
	(i.e. your function is not fitting over the data, it has a "bias")
high variance: dev set erro >> train set error -> overfitting (are we doing a good job generalizing? i.e. is our sample a representative of the population?)


Baysian erro: optimal error (the best we can do)



Andrew's basic recipie for machine learning:
1) High bias? (training data performance)
	1.1) Yes: try a bigger network, train longer, change to a more suitable neural network architecture...
	1.1.1) Go to step 1
	1.2) No: Go to step 2

2) Hight variance? (dev set performance)
	2.1) Yes: more data, regularization, try a more appropriate NN arch.
	2.1.1) go to step 1
	2.2) No: go to step 3

3) Done


Variance/Bias tradeoff: common in the past. Not an issue as long as we can provide more data and increase size of the deep network
	One of the reasons why deep learning has been so useful for supervised learning
	So long as we are regularizing, training a biger network will only have computation cost and doesn't hurt variance


l2 regularization: aka weight decay since the added Frobenius norm of the weights to the cost function causes the weight to decrease in magnitude:
w -> w * (1 - alpha * lambda / m)
By increasing lambda (the regularization parameter) we minimize W and therefore minimize Z and make it linear, which makes the resulting function
linear and high bias.

Frobenius norm of a matrix: sum of elements squared (for technical reasons not called the L2 norm)

Q) What if we tweek the power used in the weight matrix norm (instead of just using 2)?









