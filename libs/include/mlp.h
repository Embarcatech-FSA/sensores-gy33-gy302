#ifndef MLP_H
#define MLP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int input_layer_length;
    int hidden_layer_length;
    int output_layer_length;
    int max_epochs;
    float learning_rate;
    float threshold;
    float **hidden_layer_weights;
    float **output_layer_weights;
    float *hidden_layer_outputs;
    float *output_layer_outputs;
} MLP;

// Funções de ativação
float identity(float z);
float sigmoid(float z);
float tanhyper(float z);

// Derivadas
float d_identity(float z);
float d_sigmoid(float z);
float d_tanhyper(float z);

// Funções principais
void model(MLP* mlp, int input_layer_length, int hidden_layer_length, int output_layer_length, int max_epochs, float learning_rate, float threshold);
void forward(MLP* mlp, float* X);
void backpropagation(MLP* mlp, float** X, float** Y, int samples);

#ifdef __cplusplus
}
#endif

#endif // MLP_H
