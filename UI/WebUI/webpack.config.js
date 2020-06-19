var path = require('path');
var webpack = require('webpack');
var WebpackShellPlugin = require('webpack-shell-plugin');
const UglifyJsPlugin = require('uglifyjs-webpack-plugin');
const CompressionPlugin = require('compression-webpack-plugin');

//console.log(process.env.FOO);
//process.env.NODE_ENV = "production";

var PluginSets = [];
var opt_minimizer = [];

if(process.env.NODE_ENV === "production")
{
  PluginSets.push(new webpack.optimize.OccurrenceOrderPlugin());
  PluginSets.push(new webpack.DefinePlugin({
    'process.env': {
      'NODE_ENV': JSON.stringify('production')
    }
  })); 
  opt_minimizer.push(new UglifyJsPlugin({
    cache: true,
    parallel: true,
  }));
  opt_minimizer.push(new CompressionPlugin({
    test: /\.js(\?.*)?$/i,
  }));

}
if(process.env.NOTIMON_PRJ === "deploy")
{
  PluginSets.push(new WebpackShellPlugin({
    onBuildStart: ['echo "WebpackShellPlugin is working"'],
    onBuildEnd: ['echo "Start to deploy to Android proj" && sh deployToAndroidUI.sh && echo "deploy OK"']
  }));
}

module.exports = {
  cache: true,
  entry: './src/script.jsx',
  output: { 
    path: path.resolve(__dirname, 'dist'),
    filename: '[name].js', 
    publicPath: '/dist/'},
  devtool: (process.env.NODE_ENV !== "production")?"cheap-module-eval-source-map" : undefined,
  plugins:PluginSets,
  // target: 'electron-renderer',
  optimization: {
    splitChunks: {
      chunks: 'all',
    },
  },
  resolve: {
    alias: {
      UTIL: path.resolve(__dirname, 'src/UTIL/'),
      JSSRCROOT: path.resolve(__dirname, 'src/'),
      REDUX_STORE_SRC: path.resolve(__dirname, 'src/redux/'),
      STYLE: path.resolve(__dirname, 'style/')
    }
  },
  module: {
    rules: [
      {

        test: /.js.?$/,
        exclude: /node_modules/,
        use: {
          loader: 'babel-loader',
          options: {
            presets: ["@babel/preset-env", "@babel/preset-react"]
          }
        }
        // query: {
        //   cacheDirectory: true, //important for performance
        //   presets: ['es2015', 'react']
        // }
      },
      { test: /\.css$/, use:  [
                    'style-loader',  // 這個會後執行 (順序很重要)
                    'css-loader' // 這個會先執行
                ]},
      { test: /\.(png|woff|woff2|eot|ttf|svg)$/, use: 'url-loader?limit=100000' },

      {
        test: /\.less$/,
        use: [{
          loader: 'style-loader',
        }, {
          loader: 'css-loader', // translates CSS into CommonJS
        }, {  
          loader: 'less-loader', // compiles Less to CSS
          options: {
            lessOptions: {
              modifyVars: {
                'primary-color': '#82CBCB',
                'link-color': '#1DA57A',
                'border-radius-base': '2px',
              },
              javascriptEnabled: true
            }

              
          }
        }]
      }]
  },
};