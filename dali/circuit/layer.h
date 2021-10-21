//
// Created by Yihang Yang on 11/28/19.
//

#ifndef DALI_DALI_CIRCUIT_LAYER_H_
#define DALI_DALI_CIRCUIT_LAYER_H_

#include <string>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "status.h"

/****
 * This header file contains class Layer, which is an abstract class for MetalLayer
 * and WellLayer. All these classes are defined in this file.
 * ****/

namespace dali {

/****
 * This is an abstract class of any layers, this class contains:
 *     width: min width of this layer
 *     spacing: min spacing of this layer
 */
class Layer {
  public:
    Layer();
    Layer(double width, double spacing);

    // set the min width
    void SetWidth(double width);

    // get the min width
    double Width() const { return width_; }

    // set the min spacing
    void SetSpacing(double spacing);

    // get the min spacing
    double Spacing() const { return spacing_; }
  protected:
    double width_;
    double spacing_;
};

/****
 * This is a class for metal layers, which contains basic information for
 * placement, mainly I/O placement.
 */
class MetalLayer : public Layer {
  public:
    explicit MetalLayer(std::pair<const std::string, int> *name_id_pair_ptr);
    MetalLayer(
        double width,
        double spacing,
        std::pair<const std::string, int> *name_id_pair_ptr,
        MetalDirection direction = HORIZONTAL
    );

    // get the name of this layer
    const std::string &Name() const;

    // get the index of this layer
    int Id() const;

    // set the min area
    void SetMinArea(double min_area);

    // get the min area
    double MinArea() const;

    // compute and get the min height
    double MinHeight() const;

    // set the pitch of this layer, this can be different from min_width + min_spacing
    void SetPitch(double x_pitch, double y_pitch);

    // automatically compute pitches from min_width and min_spacing
    void SetPitchUsingWidthSpacing();

    // get the pitch along x direction
    double PitchX() const;

    // get the pitch along y direction
    double PitchY() const;

    // set direction of this metal layer
    void SetDirection(MetalDirection direction);

    // get direction of this metal layer
    MetalDirection Direction() const;

    // get the string for the direction of this layer
    std::string DirectionStr() const;

    // print information of this layer
    void Report() const;
  private:
    std::pair<const std::string, int> *name_id_pair_ptr_;
    double min_area_;
    double x_pitch_;
    double y_pitch_;
    MetalDirection direction_;
};

/****
 * This is a class for N/P-well layers, which contains basic information for
 * placement, mainly well legalization.
 */
class WellLayer : public Layer {
  public:
    WellLayer();
    WellLayer(
        double width,
        double spacing,
        double opposite_spacing,
        double max_plug_dist,
        double overhang
    );

    // set all parameters
    void SetParams(
        double width,
        double spacing,
        double opposite_spacing,
        double max_plug_dist,
        double overhang
    );

    // set opposite spacing
    void SetOppositeSpacing(double opposite_spacing);

    // get opposite spacing
    double OppositeSpacing() const;

    // set max plug distance
    void SetMaxPlugDist(double max_plug_dist);

    // get max plug distance
    double MaxPlugDist() const;

    // set overhang
    void SetOverhang(double overhang);

    // get overhang
    double Overhang() const;

    // print information of this layer
    void Report() const;
  private:
    double opposite_spacing_;
    double max_plug_dist_;
    double overhang_;
};

}

#endif //DALI_DALI_CIRCUIT_LAYER_H_
