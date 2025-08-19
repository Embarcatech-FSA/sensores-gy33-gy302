#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "mlp.h"

float identity(float z) {
	return z;
}

float sigmoid(float z) {
	return (1.0/(1.0 + expf(-z)));
}

float tanhyper(float z) {
	return tanhf(z);
}

float d_identity(float z) {
	return 1.0;
}

float d_sigmoid(float z) {
	return (z*(1.0 - z));
}

float d_tanhyper(float z) {
	return 1.0 - z * z;
}

void model(MLP* mlp, int input_layer_length, int hidden_layer_length, int output_layer_length, int max_epochs, float learning_rate, float threshold) {
	mlp->input_layer_length = input_layer_length;
	mlp->hidden_layer_length = hidden_layer_length;
	mlp->output_layer_length = output_layer_length;
	mlp->max_epochs = max_epochs;
	mlp->learning_rate = learning_rate;
	mlp->threshold = threshold;

	srand(time(0));

	mlp->hidden_layer_weights = malloc(hidden_layer_length * sizeof(float*));

	for(int i = 0; i < hidden_layer_length; i++) {
		mlp->hidden_layer_weights[i] = malloc((input_layer_length + 1) * sizeof(float));
		for(int j = 0; j < (input_layer_length + 1); j++) {
			mlp->hidden_layer_weights[i][j] = 2.0f * ((float)rand() / (2.0f * (float)RAND_MAX)) - 0.5f;
		}
	}

	mlp->output_layer_weights = malloc(output_layer_length * sizeof(float*));

	for(int i = 0; i < output_layer_length; i++) {
		mlp->output_layer_weights[i] = malloc((hidden_layer_length + 1) * sizeof(float));
		for(int j = 0; j < (hidden_layer_length + 1); j++) {
			mlp->output_layer_weights[i][j] = 2.0f * ((float)rand() / (2.0f * (float)RAND_MAX)) - 0.5f;
		}
	}

	mlp->hidden_layer_outputs = malloc(hidden_layer_length * sizeof(float));
	mlp->output_layer_outputs = malloc(output_layer_length * sizeof(float));
}

void forward(MLP* mlp, float* X) {
	float hidden_layer_net = 0, output_layer_net = 0;

	for(int i = 0; i < mlp->hidden_layer_length; i++) {
		hidden_layer_net = 0;
		for(int j = 0; j < (mlp->input_layer_length); j++) {
			hidden_layer_net += mlp->hidden_layer_weights[i][j] * X[j];
		}
		hidden_layer_net += mlp->hidden_layer_weights[i][mlp->input_layer_length];
		mlp->hidden_layer_outputs[i] = sigmoid(hidden_layer_net);
	}

	for(int i = 0; i < mlp->output_layer_length ; i++) {
		output_layer_net = 0;
		for(int j = 0; j < mlp->hidden_layer_length; j++) {
			output_layer_net += mlp->output_layer_weights[i][j] * mlp->hidden_layer_outputs[j];
		}
		output_layer_net += mlp->output_layer_weights[i][mlp->hidden_layer_length];
		mlp->output_layer_outputs[i] = sigmoid(output_layer_net);
	}
}

void backpropagation(MLP* mlp, float** X, float** Y, int samples) {
	int epoch = 0;
	float quad_error = 2*mlp->threshold;
	float delta_hidden_layer[mlp->hidden_layer_length], delta_output_layer[mlp->output_layer_length];
	
	while(quad_error > mlp->threshold && epoch < mlp->max_epochs) {
		quad_error = 0;

		for(int i = 0; i < samples; i++) {
			forward(mlp, X[i]);
		
			for(int j = 0; j < mlp->output_layer_length; j++) {
				float error = Y[i][j] - mlp->output_layer_outputs[j];
				
				quad_error += pow(error, 2);

				for(int k  = 0; k < mlp->hidden_layer_length; k++) {
					mlp->output_layer_weights[j][k] -= mlp->learning_rate * - 2 * error * d_sigmoid(mlp->output_layer_outputs[j]) * mlp->hidden_layer_outputs[k];
				}
				mlp->output_layer_weights[j][mlp->hidden_layer_length] -= mlp->learning_rate * - 2 * error * d_sigmoid(mlp->output_layer_outputs[j]) * 1;
			}

			for(int j = 0; j < mlp->hidden_layer_length; j++) {
				float sum = 0;

				for(int k  = 0; k < mlp->output_layer_length; k++) {
					float error = Y[i][k] - mlp->output_layer_outputs[k];

					sum += - 2 * error * d_sigmoid(mlp->output_layer_outputs[k]) * mlp->output_layer_weights[k][j];
				}

				for(int k  = 0; k < mlp->input_layer_length; k++) {
					mlp->hidden_layer_weights[j][k] -= mlp->learning_rate * sum * d_sigmoid(mlp->hidden_layer_outputs[j]) * X[i][k];
				}
				mlp->hidden_layer_weights[j][mlp->input_layer_length] -= mlp->learning_rate * sum * d_sigmoid(mlp->hidden_layer_outputs[j]) ;
			}
		}
		
		quad_error = quad_error/samples;
		epoch++;

        printf("Erro medio: %f\n", quad_error);
	}
}