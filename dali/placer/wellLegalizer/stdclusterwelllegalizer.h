//
// Created by Yihang Yang on 3/14/20.
//

#ifndef DALI_SRC_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_
#define DALI_SRC_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_

#include "blockcluster.h"
#include "blocksegment.h"
#include "cluster.h"
#include "dali/circuit/block.h"
#include "dali/common/misc.h"
#include "dali/placer/legalizer/LGTetrisEx.h"
#include "dali/placer/placer.h"
#include "stripe.h"

namespace dali {

class StdClusterWellLegalizer : public Placer {
    friend class Dali;
  private:
    bool is_first_row_orient_N_ = true;

    /**** well parameters ****/
    int max_unplug_length_;
    int well_tap_cell_width_;
    int well_spacing_;

    /**** stripe parameters ****/
    int max_cell_width_;
    double stripe_width_factor_ = 2.0;
    int stripe_width_;
    int tot_col_num_;
    StripePartitionMode stripe_mode_ = STRICT;

    /**** well tap cell parameters ****/
    BlockType *well_tap_cell_;
    int tap_cell_p_height_;
    int tap_cell_n_height_;
    int space_to_well_tap_ = 1;

    std::vector<IndexLocPair<int>> index_loc_list_; // list of index loc pair for location sort
    std::vector<ClusterStripe> col_list_; // list of stripes

    /**** row information ****/
    bool row_height_set_;
    int row_height_;
    int tot_num_rows_;
    std::vector<std::vector<SegI>> white_space_in_rows_; // white space in each row

    /**** parameters for legalization ****/
    int max_iter_ = 10;

    /**** initial location ****/
    std::vector<int2d> block_init_locations_;

  public:
    StdClusterWellLegalizer();
    void LoadConf(std::string const &config_file) override;

    void CheckWellExistence();

    void SetStripePartitionMode(StripePartitionMode mode) { stripe_mode_ = mode; }

    void SetRowHeight(int row_height) {
        DaliExpects(row_height > 0,
                    "Setting row height to a negative value? StdClusterWellLegalizer::SetRowHeight()\n");
        row_height_set_ = true;
        row_height_ = row_height;
    }
    int StartRow(int y_loc) { return (y_loc - bottom_) / row_height_; }
    int EndRow(int y_loc) {
        int relative_y = y_loc - bottom_;
        int res = relative_y / row_height_;
        if (relative_y % row_height_ == 0) {
            --res;
        }
        return res;
    }
    int RowToLoc(int row_num, int displacement = 0) { return row_num * row_height_ + bottom_ + displacement; }
    void SetFirstRowOrientN(bool is_N) { is_first_row_orient_N_ = is_N; }
    int LocToCol(int x) {
        int col_num = (x - RegionLeft()) / stripe_width_;
        if (col_num < 0) {
            col_num = 0;
        }
        if (col_num >= tot_col_num_) {
            col_num = tot_col_num_ - 1;
        }
        return col_num;
    }

    void DetectAvailSpace();
    void FetchNPWellParams();
    void UpdateWhiteSpaceInCol(ClusterStripe &col);
    void DecomposeToSimpleStripe();

    void SaveInitialBlockLocation();

    void Initialize(int cluster_width = 0);

    void AssignBlockToColBasedOnWhiteSpace();

    void AppendBlockToColBottomUp(Stripe &stripe, Block &blk);
    void AppendBlockToColTopDown(Stripe &stripe, Block &blk);
    void AppendBlockToColBottomUpCompact(Stripe &stripe, Block &blk);
    void AppendBlockToColTopDownCompact(Stripe &stripe, Block &blk);

    bool StripeLegalizationBottomUp(Stripe &stripe);
    bool StripeLegalizationTopDown(Stripe &stripe);
    bool StripeLegalizationBottomUpCompact(Stripe &stripe);
    bool StripeLegalizationTopDownCompact(Stripe &stripe);

    bool BlockClustering();
    bool BlockClusteringLoose();
    bool BlockClusteringCompact();

    bool TrialClusterLegalization(Stripe &stripe);

    double WireLengthCost(Cluster *cluster, int l, int r);
    void FindBestLocalOrder(std::vector<Block *> &res,
                            double &cost,
                            Cluster *cluster,
                            int cur,
                            int l,
                            int r,
                            int left_bound,
                            int right_bound,
                            int gap,
                            int range);
    void LocalReorderInCluster(Cluster *cluster, int range = 3);
    void LocalReorderAllClusters();

    //void SingleSegmentClusteringOptimization();

    void UpdateClusterOrient();
    void InsertWellTap();

    void ClearCachedData();
    bool WellLegalize();

    bool StartPlacement() override;

    void ReportEffectiveSpaceUtilization();

    /****member function for file IO****/
    void GenMatlabClusterTable(std::string const &name_of_file);
    void GenMATLABWellTable(std::string const &name_of_file, int well_emit_mode) override;
    void GenPPNP(std::string const &name_of_file);
    void EmitDEFWellFile(std::string const &name_of_file, int well_emit_mode) override;
    void EmitPPNPRect(std::string const &name_of_file);
    void EmitWellRect(std::string const &name_of_file, int well_emit_mode);
    void EmitClusterRect(std::string const &name_of_file);

    /**** member functions for debugging ****/
    void PlotAvailSpace(std::string const &name_of_file = "avail_space.txt");
    void PlotAvailSpaceInCols(std::string const &name_of_file = "avail_space.txt");
    void PlotSimpleStripes(std::string const &name_of_file = "stripe_space.txt");
};

}

#endif //DALI_SRC_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_
