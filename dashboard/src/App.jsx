import React, { useState, useEffect, useRef } from 'react';
import mqtt from 'mqtt';

// ⚙️ CONFIGURATION
const MQTT_BROKER = 'ws://broker.hivemq.com:8000/mqtt';

// ⚠️ Topics
const TOPIC_DATA = 'cmu/iot/benz/server-room/data'; 
const TOPIC_COMMAND = 'cmu/iot/benz/server-room/command'; 

export default function ServerRoomDashboard() {
    const clientRef = useRef(null);
    const [connectionStatus, setConnectionStatus] = useState('Disconnected');
    
    const [data, setData] = useState({
        temperature: 0, humidity: 0, gas: 0, status: 'normal', timestamp: new Date()
    });

    const [history, setHistory] = useState([]);
    const [fanManual, setFanManual] = useState(false);
    const [dehumidifierManual, setDehumidifierManual] = useState(false);
    const [alertActive, setAlertActive] = useState(false);

    useEffect(() => {
        console.log('Connecting to MQTT Broker...');
        const mqttClient = mqtt.connect(MQTT_BROKER);

        mqttClient.on('connect', () => {
            console.log('✅ Connected to MQTT!');
            setConnectionStatus('Connected');
            mqttClient.subscribe(TOPIC_DATA);
        });

        mqttClient.on('message', (topic, message) => {
            if (topic === TOPIC_DATA) {
                try {
                    const payload = JSON.parse(message.toString());
                    
                    const newData = {
                        temperature: parseFloat(payload.temp),
                        humidity: parseFloat(payload.humi),
                        gas: parseInt(payload.gas),
                        status: payload.status,
                        timestamp: new Date()
                    };

                    setData(newData);
                    setAlertActive(payload.status !== 'normal');

                    // ✅ แก้ไขสูตรกราฟ: หาร 50 (รองรับ Scale 0-5000)
                    setHistory(prev => {
                        const newHistory = [...prev, {
                            time: newData.timestamp.toLocaleTimeString(),
                            temp: newData.temperature,
                            gas: newData.gas / 50 
                        }];
                        return newHistory.slice(-30); 
                    });

                } catch (error) {
                    console.error('Error parsing MQTT message:', error);
                }
            }
        });

        clientRef.current = mqttClient;

        return () => {
            if (mqttClient) mqttClient.end();
        };
    }, []);

    const sendCommand = (command, value) => {
        if (clientRef.current) {
            clientRef.current.publish(TOPIC_COMMAND, JSON.stringify({ command, value }));
            console.log(`📤 Sent Command: ${command} -> ${value}`);
        }
    };

    // Helper Functions for Styling
    const getStatusColor = () => data.status === 'critical' ? 'bg-red-500' : data.status === 'warning' ? 'bg-yellow-500' : 'bg-green-500';
    const getStatusEmoji = () => data.status === 'critical' ? '🔴' : data.status === 'warning' ? '⚠️' : '✅';
    
    return (
        <div className="min-h-screen bg-gradient-to-br from-indigo-600 via-purple-600 to-pink-500 p-4 font-sans">
            <div className="max-w-7xl mx-auto">
                
                {/* Header */}
                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-2xl p-6 mb-6">
                    <div className="flex justify-between mb-4">
                        <h1 className="text-3xl font-bold text-gray-800">🖥️ Smart Server Guardian</h1>
                        <div className="text-right">
                            <div className="text-sm text-gray-500">{data.timestamp.toLocaleTimeString()}</div>
                            <div className={`text-xs font-bold ${connectionStatus === 'Connected' ? 'text-green-500' : 'text-red-500'}`}>MQTT: {connectionStatus}</div>
                        </div>
                    </div>
                    <div className={`${getStatusColor()} text-white text-center py-4 rounded-2xl text-2xl font-bold transition-all`}>
                        {getStatusEmoji()} System Status: {data.status.toUpperCase()}
                    </div>

                    {alertActive && (
                        <div className="mt-4 bg-red-100 border-l-4 border-red-500 p-4 rounded animate-pulse">
                            <p className="text-red-700 font-semibold">
                                ⚠️ Alert Active - System requires attention!
                            </p>
                        </div>
                    )}
                </div>

                {/* Sensors Grid */}
                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-6">
                    {/* Temperature (แก้เกณฑ์สีเป็น 30.0 ตาม main.cpp) */}
                    <div className="bg-white/95 p-6 rounded-3xl shadow-xl">
                        <div className="text-gray-500 text-sm font-semibold mb-2">🌡️ TEMPERATURE</div>
                        <div className={`text-5xl font-bold ${data.temperature > 30 ? 'text-red-600' : data.temperature > 26 ? 'text-yellow-600' : 'text-green-600'}`}>{data.temperature.toFixed(1)}°C</div>
                        <div className="h-2 bg-gray-200 rounded-full mt-4 overflow-hidden"><div className="h-full bg-green-500 transition-all" style={{width: `${(data.temperature/50)*100}%`}}></div></div>
                    </div>
                    {/* Humidity */}
                    <div className="bg-white/95 p-6 rounded-3xl shadow-xl">
                        <div className="text-gray-500 text-sm font-semibold mb-2">💧 HUMIDITY</div>
                        <div className="text-5xl font-bold text-blue-600">{data.humidity.toFixed(1)}%</div>
                        <div className="h-2 bg-gray-200 rounded-full mt-4 overflow-hidden"><div className="h-full bg-blue-500 transition-all" style={{width: `${data.humidity}%`}}></div></div>
                    </div>
                    {/* Gas (แก้ Progress Bar หาร 5000) */}
                    <div className="bg-white/95 p-6 rounded-3xl shadow-xl">
                        <div className="text-gray-500 text-sm font-semibold mb-2">💨 GAS LEVEL</div>
                        <div className="text-5xl font-bold text-purple-600">{data.gas}</div>
                        <div className="h-2 bg-gray-200 rounded-full mt-4 overflow-hidden"><div className="h-full bg-purple-500 transition-all" style={{width: `${(data.gas/5000)*100}%`}}></div></div>
                    </div>
                </div>

                {/* Graph Section */}
                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl mb-6 p-6">
                    <h3 className="text-xl font-bold text-gray-800 mb-4">📈 Real-time Data Stream</h3>
                    
                    <div className="flex items-start">
                        {/* 1. แกน Y (Scale) */}
                        <div className="flex flex-col justify-between h-48 text-xs text-gray-400 font-mono mr-2 py-1 text-right select-none">
                            <span>50°C / 100%</span>
                            <span>37.5°C / 75%</span>
                            <span>25°C / 50%</span>
                            <span>12.5°C / 25%</span>
                            <span>0°C / 0%</span>
                        </div>

                        {/* 2. พื้นที่กราฟ */}
                        <div className="flex-1 h-48 flex justify-between gap-1 border-b border-l border-gray-200 p-2 relative items-end">
                            {history.length === 0 && <div className="absolute inset-0 flex items-center justify-center text-gray-400">Waiting for data...</div>}
                            
                            {history.map((item, i) => (
                                <div key={i} className="flex-1 h-full flex flex-row items-end justify-center gap-[1px] group relative hover:bg-gray-50/50 rounded-t">
                                    
                                    {/* Tooltip: คูณ 50 กลับคืนค่าจริง */}
                                    <div className="absolute bottom-full mb-2 hidden group-hover:block z-10 bg-gray-800 text-white text-xs p-2 rounded shadow-lg whitespace-nowrap">
                                        <div className="font-bold border-b border-gray-600 pb-1 mb-1">{item.time}</div>
                                        <div>🌡️ Temp: {item.temp.toFixed(1)}°C</div>
                                        <div>💨 Gas: {Math.round(item.gas * 50)}</div> 
                                    </div>

                                    {/* Temp Bar (Red) */}
                                    <div className="w-1.5 bg-red-400/90 rounded-t hover:bg-red-500 transition-all duration-500" 
                                         style={{ height: `${Math.min(100, (item.temp / 50) * 100)}%` }}>
                                    </div>

                                    {/* Gas Bar (Blue) */}
                                    <div className="w-1.5 bg-blue-400/90 rounded-t hover:bg-blue-500 transition-all duration-500" 
                                         style={{ height: `${Math.min(100, item.gas)}%` }}>
                                    </div>
                                    
                                </div>
                            ))}
                        </div>
                    </div>

                    <div className="flex gap-4 mt-4 text-xs text-gray-500 justify-end">
                        <span className="flex items-center gap-1"><div className="w-3 h-3 bg-red-400 rounded"></div> Temp (0-50°C)</span>
                        <span className="flex items-center gap-1"><div className="w-3 h-3 bg-blue-400 rounded"></div> Gas (0-5000 ppm)</span>
                    </div>
                </div>

                {/* Controls */}
                <div className="bg-white/95 p-6 rounded-3xl shadow-xl">
                    <h2 className="text-2xl font-bold mb-4">🎛️ Manual Controls</h2>
                    <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                        
                        {/* 1. Reset Alarm */}
                        <button onClick={() => sendCommand('RESET_ALARM', 1)} 
                                className="bg-red-500 hover:bg-red-600 text-white py-4 rounded-xl font-bold shadow transition active:scale-95">
                            🔕 Reset Alarm
                        </button>
                        
                        {/* 2. Cooling Fan */}
                        <button onClick={() => { 
                                    const newState = !fanManual;
                                    setFanManual(newState); 
                                    sendCommand('FAN_CONTROL', newState ? 1 : 0); 
                                }} 
                                className={`${fanManual ? 'bg-blue-700 ring-4 ring-blue-300' : 'bg-blue-500'} hover:bg-blue-600 text-white py-4 rounded-xl font-bold shadow transition active:scale-95`}>
                            🌀 Fan: {fanManual ? 'ON' : 'OFF'}
                        </button>

                        {/* 3. Dehumidifier */}
                        <button onClick={() => { 
                                    const newState = !dehumidifierManual;
                                    setDehumidifierManual(newState); 
                                    sendCommand('DEHUMIDIFIER_CONTROL', newState ? 1 : 0); 
                                }} 
                                className={`${dehumidifierManual ? 'bg-green-700 ring-4 ring-green-300' : 'bg-green-500'} hover:bg-green-600 text-white py-4 rounded-xl font-bold shadow transition active:scale-95`}>
                            💧 Dehumidifier: {dehumidifierManual ? 'ON' : 'OFF'}
                        </button>

                    </div>
                </div>
            </div>
        </div>
    );
}