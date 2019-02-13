var path = require('path');
var webpack = require('webpack');
var WebpackShellPlugin = require('webpack-shell-plugin');
//console.log(process.env.FOO);
//process.env.NODE_ENV = "production";

var PluginSets = [];

if(process.env.NODE_ENV === "production")
{
  PluginSets.push(new webpack.optimize.OccurenceOrderPlugin());

  PluginSets.push(new webpack.DefinePlugin({
    'process.env': {
      'NODE_ENV': JSON.stringify('production')
    }
  })); 

  PluginSets.push(new webpack.optimize.UglifyJsPlugin({
    compressor: {
      warnings: false
    }
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
  output: { path: __dirname, filename: 'bundle.js' },
  devtool: (process.env.NODE_ENV !== "production")?"cheap-module-eval-source-map" : null,
  plugins:PluginSets,
  // target: 'electron-renderer',
  
  resolve: {
    alias: {
      UTIL: path.resolve(__dirname, 'src/UTIL/'),
      REDUX_STORE_SRC: path.resolve(__dirname, 'src/redux/'),
      STYLE: path.resolve(__dirname, 'style/')
    }
  },
  module: {
    rules: [
      {

        test: /.js.?$/,
        loader: 'babel-loader',
        exclude: /node_modules/,
        query: {
          cacheDirectory: true, //important for performance
          presets: ['es2015', 'react']
        }
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
          javascriptEnabled: true
         },
        }]
      }]
  },
};
