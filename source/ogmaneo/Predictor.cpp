// ----------------------------------------------------------------------------
//  OgmaNeo
//  Copyright(c) 2016-2020 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of OgmaNeo is licensed to you under the terms described
//  in the OGMANEO_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "Predictor.h"

using namespace ogmaneo;

void Predictor::forward(
    const Int2 &pos,
    std::mt19937 &rng,
    const std::vector<const IntBuffer*> &inputCs
) {
    int maxIndex = 0;
    float maxActivation = -999999.0f;

    for (int hc = 0; hc < hiddenSize.z; hc++) {
        int hiddenIndex = address3(Int3(pos.x, pos.y, hc), hiddenSize);

        float sum = 0.0f;

        // For each visible layer
        for (int vli = 0; vli < visibleLayers.size(); vli++) {
            VisibleLayer &vl = visibleLayers[vli];
            const VisibleLayerDesc &vld = visibleLayerDescs[vli];

            sum += vl.weights.multiplyOHVs(*inputCs[vli], hiddenIndex, vld.size.z);
        }

        if (sum > maxActivation) {
            maxActivation = sum;
            maxIndex = hc;
        }
    }

    hiddenCs[address2(pos, Int2(hiddenSize.x, hiddenSize.y))] = maxIndex;
}

void Predictor::learn(
    const Int2 &pos,
    std::mt19937 &rng,
    const IntBuffer* hiddenTargetCs
) {
    int hiddenColumnIndex = address2(pos, Int2(hiddenSize.x, hiddenSize.y));

    int targetC = (*hiddenTargetCs)[hiddenColumnIndex];

    for (int hc = 0; hc < hiddenSize.z; hc++) {
        int hiddenIndex = address3(Int3(pos.x, pos.y, hc), hiddenSize);

        float sum = 0.0f;
        int count = 0;

        for (int vli = 0; vli < visibleLayers.size(); vli++) {
            VisibleLayer &vl = visibleLayers[vli];
            const VisibleLayerDesc &vld = visibleLayerDescs[vli];

            sum += vl.weights.multiplyOHVs(vl.inputCsPrev, hiddenIndex, vld.size.z);
            count += vl.weights.count(hiddenIndex) / vld.size.z;
        }

        sum /= std::max(1, count);

        float delta = alpha * ((hc == targetC ? 1.0f : -1.0f) - std::tanh(sum));

        for (int vli = 0; vli < visibleLayers.size(); vli++) {
            VisibleLayer &vl = visibleLayers[vli];
            const VisibleLayerDesc &vld = visibleLayerDescs[vli];

            vl.weights.deltaOHVs(vl.inputCsPrev, delta, hiddenIndex, vld.size.z);
        }
    }
}

void Predictor::initRandom(
    ComputeSystem &cs,
    const Int3 &hiddenSize,
    const std::vector<VisibleLayerDesc> &visibleLayerDescs
) {
    this->visibleLayerDescs = visibleLayerDescs;

    this->hiddenSize = hiddenSize;

    visibleLayers.resize(visibleLayerDescs.size());

    // Pre-compute dimensions
    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    std::uniform_real_distribution<float> weightDist(-0.01f, 0.01f);

    // Create layers
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        VisibleLayer &vl = visibleLayers[vli];
        VisibleLayerDesc &vld = this->visibleLayerDescs[vli];

        int numVisibleColumns = vld.size.x * vld.size.y;

        // Create weight matrix for this visible layer and initialize randomly
        initSMLocalRF(vld.size, hiddenSize, vld.radius, vl.weights);

        for (int i = 0; i < vl.weights.nonZeroValues.size(); i++)
            vl.weights.nonZeroValues[i] = weightDist(cs.rng);

        vl.inputCsPrev = IntBuffer(numVisibleColumns, 0);
    }

    // Hidden Cs
    hiddenCs = IntBuffer(numHiddenColumns, 0);
}

void Predictor::activate(
    ComputeSystem &cs,
    const std::vector<const IntBuffer*> &inputCs
) {
    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    // Forward kernel
#ifdef KERNEL_NO_THREAD
    for (int x = 0; x < hiddenSize.x; x++)
        for (int y = 0; y < hiddenSize.y; y++)
            forward(Int2(x, y), cs.rng, inputCs);
#else
    runKernel2(cs, std::bind(Predictor::forwardKernel, std::placeholders::_1, std::placeholders::_2, this, inputCs), Int2(hiddenSize.x, hiddenSize.y), cs.rng, cs.batchSize2);
#endif

    // Copy to prevs
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        VisibleLayer &vl = visibleLayers[vli];
        VisibleLayerDesc &vld = visibleLayerDescs[vli];

        int numVisibleColumns = vld.size.x * vld.size.y;

#ifdef KERNEL_NO_THREAD
        for (int x = 0; x < numVisibleColumns; x++)
            copyInt(x, cs.rng, inputCs[vli], &vl.inputCsPrev);
#else
        runKernel1(cs, std::bind(copyInt, std::placeholders::_1, std::placeholders::_2, inputCs[vli], &vl.inputCsPrev), numVisibleColumns, cs.rng, cs.batchSize1);
#endif
    }
}

void Predictor::learn(
    ComputeSystem &cs,
    const IntBuffer* hiddenTargetCs
) {
    // Learn kernel
#ifdef KERNEL_NO_THREAD
    for (int x = 0; x < hiddenSize.x; x++)
        for (int y = 0; y < hiddenSize.y; y++)
            learn(Int2(x, y), cs.rng, hiddenTargetCs);
#else
    runKernel2(cs, std::bind(Predictor::learnKernel, std::placeholders::_1, std::placeholders::_2, this, hiddenTargetCs), Int2(hiddenSize.x, hiddenSize.y), cs.rng, cs.batchSize2);
#endif
}

void Predictor::writeToStream(
    std::ostream &os
) const {
    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    os.write(reinterpret_cast<const char*>(&hiddenSize), sizeof(Int3));

    os.write(reinterpret_cast<const char*>(&alpha), sizeof(float));

    writeBufferToStream(os, &hiddenCs);

    int numVisibleLayers = visibleLayers.size();

    os.write(reinterpret_cast<char*>(&numVisibleLayers), sizeof(int));
    
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        const VisibleLayer &vl = visibleLayers[vli];
        const VisibleLayerDesc &vld = visibleLayerDescs[vli];

        os.write(reinterpret_cast<const char*>(&vld), sizeof(VisibleLayerDesc));

        writeSMToStream(os, vl.weights);

        writeBufferToStream(os, &vl.inputCsPrev);
    }
}

void Predictor::readFromStream(
    std::istream &is
) {
    is.read(reinterpret_cast<char*>(&hiddenSize), sizeof(Int3));

    int numHiddenColumns = hiddenSize.x * hiddenSize.y;
    int numHidden = numHiddenColumns * hiddenSize.z;

    is.read(reinterpret_cast<char*>(&alpha), sizeof(float));

    readBufferFromStream(is, &hiddenCs);

    int numVisibleLayers;
    
    is.read(reinterpret_cast<char*>(&numVisibleLayers), sizeof(int));

    visibleLayers.resize(numVisibleLayers);
    visibleLayerDescs.resize(numVisibleLayers);
    
    for (int vli = 0; vli < visibleLayers.size(); vli++) {
        VisibleLayer &vl = visibleLayers[vli];
        VisibleLayerDesc &vld = visibleLayerDescs[vli];

        is.read(reinterpret_cast<char*>(&vld), sizeof(VisibleLayerDesc));

        readSMFromStream(is, vl.weights);

        readBufferFromStream(is, &vl.inputCsPrev);
    }
}