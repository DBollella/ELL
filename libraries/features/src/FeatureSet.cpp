////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Machine Learning Library (EMLL)
//  File:     FeatureSet.cpp (features)
//  Authors:  Chuck Jacobs
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FeatureSet.h"
#include "Feature.h"
#include "InputFeature.h"
#include "StringUtil.h"
#include "DoubleVector.h"

#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <unordered_map>

namespace features
{
    //
    // FeatureSet class
    //

    void FeatureSet::Reset()
    {
        _numItemsProcessed = 0;
        if (_outputFeature)
        {
            _outputFeature->Reset();
        }
    }

    bool FeatureSet::ProcessInputData(const DataVector& inst) const
    {
        if (_inputFeature && _outputFeature)
        {
            _inputFeature->SetValue(inst);
            auto newFeatures = _outputFeature->GetOutput();
            auto hasOutput = HasOutput();
            if(_numItemsProcessed < _outputFeature->GetWarmupTime())
            {
                ++_numItemsProcessed;
            }
            return hasOutput;
        }

        return false;
    }
    
    Feature* FeatureSet::GetFeature(const std::string& featureId) const
    {
        auto it = _featureMap.find(featureId);
        if (it != _featureMap.end())
        {
            return it->second;
        }
        else
        {
            return nullptr;
        }
    }

    Feature* FeatureSet::CreateFeatureFromDescription(const std::vector<std::string>& description)
    {
        _features.emplace_back(Feature::FromDescription(description, _featureMap));
        auto feature = _features.back().get();
        if(_featureMap.size() == 0)
        {
            if(dynamic_cast<InputFeature*>(feature) != nullptr)
            {
                _inputFeature = dynamic_cast<InputFeature*>(feature);
            }
            else
            {
                throw std::runtime_error("First feature must be input feature");
            }
        }

        // add to map
        _featureMap[feature->Id()] = feature;
        return feature;
    }

    layers::CoordinateList FeatureSet::AddToModel(layers::Model& model, const layers::CoordinateList& inputCoordinates) const
    {
        if(_inputFeature == nullptr) 
        {
            throw std::runtime_error("Input feature not assigned");
        }
        if(_outputFeature == nullptr)
        {
            throw std::runtime_error("Output feature not assigned");
        }
        
        // need to keep a map of output coordinate lists for the various features
        std::unordered_map<const Feature*, layers::CoordinateList> featureOutputs;
        layers::CoordinateList outputCoordinates;
        auto modelAdder = [this, &inputCoordinates, &featureOutputs, &outputCoordinates, &model](const Feature& f)
        {
            auto featurePtr = &f; // bleh
            layers::CoordinateList coordinates;
            if(featurePtr == _inputFeature)
            {
                coordinates = inputCoordinates;
            }
            else
            {
                coordinates = f.AddToModel(model, featureOutputs);
            }

            featureOutputs.emplace(featurePtr, coordinates);
            if(featurePtr == _outputFeature)
            {
                outputCoordinates = coordinates;
            }
        };

        // traverse graph, adding layers --- visit graph in same way we do during compute
        Visit(modelAdder);
        
        return outputCoordinates;
    }


// TODO: XML serialization
// TODO: for variance feature, have 2 constructors, one which makes an anonymous mean feature
// TODO: put Unary, Binary feature base classes in Feature.h
} // end namepsace