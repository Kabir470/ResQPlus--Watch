import axios from 'axios';

const apiClient = axios.create({
  baseURL: 'https://your-backend-api.com/api', // Replace with your actual backend URL
  timeout: 10000,
});

export default apiClient;
