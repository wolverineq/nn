#include "nn/pred.h"
#include <fstream>
#include "opt/opt.h"

namespace rnn {

    pred_param_t load_pred_param(std::istream& is)
    {
        pred_param_t result;
        std::string line;

        ebt::json::json_parser<la::matrix<double>> mat_parser;
        ebt::json::json_parser<la::vector<double>> vec_parser;

        result.softmax_weight = mat_parser.parse(is);
        std::getline(is, line);
        result.softmax_bias = vec_parser.parse(is);
        std::getline(is, line);

        return result;
    }

    pred_param_t load_pred_param(std::string filename)
    {
        std::ifstream ifs { filename };

        return load_pred_param(ifs);
    }

    void save_pred_param(pred_param_t const& param, std::ostream& os)
    {
        ebt::json::dump(param.softmax_weight, os);
        os << std::endl;
        ebt::json::dump(param.softmax_bias, os);
        os << std::endl;
    }

    void save_pred_param(pred_param_t const& param, std::string filename)
    {
        std::ofstream ofs { filename };

        save_pred_param(param, ofs);
    }

    void adagrad_update(pred_param_t& param, pred_param_t const& grad,
        pred_param_t& opt_data, double step_size)
    {
        opt::adagrad_update(param.softmax_weight, grad.softmax_weight,
            opt_data.softmax_weight, step_size);
        opt::adagrad_update(param.softmax_bias, grad.softmax_bias,
            opt_data.softmax_bias, step_size);
    }

    void rmsprop_update(pred_param_t& param, pred_param_t const& grad,
        pred_param_t& opt_data, double decay, double step_size)
    {
        opt::rmsprop_update(param.softmax_weight, grad.softmax_weight,
            opt_data.softmax_weight, decay, step_size);
        opt::rmsprop_update(param.softmax_bias, grad.softmax_bias,
            opt_data.softmax_bias, decay, step_size);
    }

    pred_nn_t make_pred_nn(autodiff::computation_graph& g,
        pred_param_t const& param,
        std::vector<std::shared_ptr<autodiff::op_t>> const& feat)
    {
        pred_nn_t result;

        result.softmax_weight = g.var(param.softmax_weight);
        result.softmax_bias = g.var(param.softmax_bias);

        for (int i = 0; i < feat.size(); ++i) {
            result.logprob.push_back(autodiff::logsoftmax(autodiff::add(
                autodiff::mul(result.softmax_weight, feat[i]), result.softmax_bias)));
        }

        return result;
    }

    pred_param_t copy_grad(pred_nn_t const& nn)
    {
        pred_param_t result;

        result.softmax_weight = autodiff::get_grad<la::matrix<double>>(nn.softmax_weight);
        result.softmax_bias = autodiff::get_grad<la::vector<double>>(nn.softmax_bias);

        return result;
    }

    void eval(pred_nn_t& nn)
    {
        std::vector<std::shared_ptr<autodiff::op_t>> order
            = autodiff::topo_order(nn.logprob);

        autodiff::eval(order, autodiff::eval_funcs);
    }

    void grad(pred_nn_t& nn)
    {
        std::vector<std::shared_ptr<autodiff::op_t>> order
            = autodiff::topo_order(nn.logprob);

        autodiff::grad(order, autodiff::grad_funcs);
    }

    double log_loss::loss()
    {
        return -la::dot(gold, pred);
    }

    la::vector<double> log_loss::grad()
    {
        return la::mul(gold, -1);
    }

    std::vector<std::shared_ptr<autodiff::op_t>> subsample_input(
        std::vector<std::shared_ptr<autodiff::op_t>> const& inputs,
        int freq)
    {
        std::vector<std::shared_ptr<autodiff::op_t>> result;
    
        for (int i = 0; i < inputs.size(); ++i) {
            if (i % freq == 0) {
                result.push_back(inputs[i]);
            }
        }
    
        return result;
    }
    
    std::vector<std::shared_ptr<autodiff::op_t>> upsample_output(
        std::vector<std::shared_ptr<autodiff::op_t>> const& outputs,
        int freq, int size)
    {
        std::vector<std::shared_ptr<autodiff::op_t>> result;
    
        std::shared_ptr<autodiff::op_t> c;
        int j = 0;
    
        for (int i = 0; i < size; ++i) {
            if (i % freq == 0) {
                c = outputs[j];
                ++j;
            }
    
            result.push_back(autodiff::add(std::vector<std::shared_ptr<autodiff::op_t>> { c }));
        }
    
        assert(j == outputs.size());
    
        return result;
    }

}