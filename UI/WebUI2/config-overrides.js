const ReactRefreshWebpackPlugin = require('@pmmmwh/react-refresh-webpack-plugin');

module.exports = function override(config, env) {
  if (env === 'development') {
    // Enable Fast Refresh for hot reloading
    config.plugins.push(new ReactRefreshWebpackPlugin());

    config.devServer = {
      ...config.devServer,
      hot: true,
      client: {
        logging: 'info',
        overlay: true,
      },
    };
  }
  return config;
};