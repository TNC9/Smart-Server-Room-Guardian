import React, { useState, useEffect } from 'react';

export default function ServerRoomDashboard() {
    const [data, setData] = useState({
        temperature: 25.0,
        humidity: 50.0,
        gas: 500,
        status: 'normal',
        timestamp: new Date()
    });

    const [history, setHistory] = useState([]);
    const [fanManual, setFanManual] = useState(false);
    const [alertActive, setAlertActive] = useState(false);

    // Simulate sensor readings
    useEffect(() => {
        const interval = setInterval(() => {
            const hour = new Date().getSeconds() % 24;
            const baseTemp = 25 + Math.sin(hour / 24 * Math.PI * 2) * 5;
            const tempSpike = Math.random() > 0.9 ? Math.random() * 15 : 0;
            const temp = baseTemp + tempSpike + (Math.random() * 2 - 1);
            const humi = 70 - (temp - 25) * 2 + (Math.random() * 10 - 5);
            const gasSpike = Math.random() > 0.95 ? Math.random() * 2000 : 0;
            const gas = Math.floor(500 + Math.random() * 1000 + gasSpike);

            let status = 'normal';
            if (temp > 40 || gas > 3000) {
                status = 'critical';
            } else if (temp > 35 || humi < 30 || humi > 70 || gas > 2000) {
                status = 'warning';
            }

            const newData = {
                temperature: Math.max(20, Math.min(50, temp)),
                humidity: Math.max(20, Math.min(90, humi)),
                gas: Math.max(0, Math.min(4095, gas)),
                status,
                timestamp: new Date()
            };

            setData(newData);
            setAlertActive(status !== 'normal');

            setHistory(prev => [...prev.slice(-29), {
                time: newData.timestamp.toLocaleTimeString(),
                temp: newData.temperature,
                humi: newData.humidity,
                gas: newData.gas / 41
            }]);
        }, 2000);

        return () => clearInterval(interval);
    }, []);

    const getStatusColor = () => {
        switch (data.status) {
            case 'critical': return 'bg-red-500';
            case 'warning': return 'bg-yellow-500';
            default: return 'bg-green-500';
        }
    };

    const getStatusEmoji = () => {
        switch (data.status) {
            case 'critical': return '🔴';
            case 'warning': return '⚠️';
            default: return '✅';
        }
    };

    const getStatusText = () => {
        switch (data.status) {
            case 'critical': return 'CRITICAL!';
            case 'warning': return 'WARNING';
            default: return 'NORMAL';
        }
    };

    const getTempColor = () => {
        if (data.temperature > 40) return 'text-red-600';
        if (data.temperature > 35) return 'text-yellow-600';
        return 'text-green-600';
    };

    const getHumiColor = () => {
        if (data.humidity < 30 || data.humidity > 70) return 'text-yellow-600';
        return 'text-green-600';
    };

    const getGasColor = () => {
        if (data.gas > 3000) return 'text-red-600';
        if (data.gas > 2000) return 'text-yellow-600';
        return 'text-green-600';
    };

    return (
        <div className="min-h-screen bg-gradient-to-br from-indigo-600 via-purple-600 to-pink-500 p-4">
            <div className="max-w-7xl mx-auto">

                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-2xl p-6 mb-6">
                    <div className="flex items-center justify-between mb-4">
                        <h1 className="text-3xl font-bold text-gray-800">
                            🖥️ Smart Server Room Guardian
                        </h1>
                        <div className="text-sm text-gray-500">
                            {data.timestamp.toLocaleTimeString()}
                        </div>
                    </div>

                    <div className={`${getStatusColor()} text-white text-center py-4 rounded-2xl text-2xl font-bold`}>
                        {getStatusEmoji()} {getStatusText()}
                    </div>

                    {alertActive && (
                        <div className="mt-4 bg-red-100 border-l-4 border-red-500 p-4 rounded">
                            <p className="text-red-700 font-semibold">
                                ⚠️ Alert Active - System requires attention!
                            </p>
                        </div>
                    )}
                </div>

                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-6">

                    <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6 transform hover:scale-105 transition">
                        <div className="flex items-center justify-between mb-4">
                            <div className="text-gray-500 text-sm font-semibold">🌡️ TEMPERATURE</div>
                            {data.temperature > 35 && (
                                <span className="text-red-500 text-xs font-bold animate-bounce">
                                    {data.temperature > 40 ? 'CRITICAL' : 'HIGH'}
                                </span>
                            )}
                        </div>
                        <div className={`text-5xl font-bold ${getTempColor()} mb-2`}>
                            {data.temperature.toFixed(1)}
                            <span className="text-2xl text-gray-400 ml-2">°C</span>
                        </div>
                        <div className="text-xs text-gray-500">
                            Warning: &gt;35°C | Critical: &gt;40°C
                        </div>
                        <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                            <div
                                className={`h-full ${getTempColor().replace('text', 'bg')} transition-all`}
                                style={{ width: `${Math.min(100, (data.temperature / 50) * 100)}%` }}
                            />
                        </div>
                    </div>

                    <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6 transform hover:scale-105 transition">
                        <div className="flex items-center justify-between mb-4">
                            <div className="text-gray-500 text-sm font-semibold">💧 HUMIDITY</div>
                            {(data.humidity < 30 || data.humidity > 70) && (
                                <span className="text-yellow-500 text-xs font-bold animate-bounce">
                                    {data.humidity < 30 ? 'TOO LOW' : 'TOO HIGH'}
                                </span>
                            )}
                        </div>
                        <div className={`text-5xl font-bold ${getHumiColor()} mb-2`}>
                            {data.humidity.toFixed(1)}
                            <span className="text-2xl text-gray-400 ml-2">%</span>
                        </div>
                        <div className="text-xs text-gray-500">
                            Range: 30-70% RH
                        </div>
                        <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                            <div
                                className={`h-full ${getHumiColor().replace('text', 'bg')} transition-all`}
                                style={{ width: `${data.humidity}%` }}
                            />
                        </div>
                    </div>

                    <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6 transform hover:scale-105 transition">
                        <div className="flex items-center justify-between mb-4">
                            <div className="text-gray-500 text-sm font-semibold">💨 GAS LEVEL</div>
                            {data.gas > 2000 && (
                                <span className={`${data.gas > 3000 ? 'text-red-500' : 'text-yellow-500'} text-xs font-bold animate-bounce`}>
                                    {data.gas > 3000 ? 'DANGER' : 'DETECTED'}
                                </span>
                            )}
                        </div>
                        <div className={`text-5xl font-bold ${getGasColor()} mb-2`}>
                            {data.gas}
                            <span className="text-2xl text-gray-400 ml-2">/4095</span>
                        </div>
                        <div className="text-xs text-gray-500">
                            Warning: &gt;2000 | Critical: &gt;3000
                        </div>
                        <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                            <div
                                className={`h-full ${getGasColor().replace('text', 'bg')} transition-all`}
                                style={{ width: `${(data.gas / 4095) * 100}%` }}
                            />
                        </div>
                    </div>
                </div>

                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6 mb-6">
                    <h3 className="text-xl font-bold text-gray-800 mb-4">📈 Historical Data</h3>
                    <div className="h-48 flex items-end justify-between gap-1">
                        {history.map((item, i) => (
                            <div key={i} className="flex-1 flex flex-col items-center gap-1">
                                <div
                                    className="w-full bg-gradient-to-t from-red-500 to-red-300 rounded-t"
                                    style={{ height: `${(item.temp / 50) * 100}%` }}
                                    title={`Temp: ${item.temp.toFixed(1)}°C`}
                                />
                                <div
                                    className="w-full bg-gradient-to-t from-blue-500 to-blue-300 rounded-t"
                                    style={{ height: `${item.gas}%` }}
                                    title={`Gas: ${Math.floor(item.gas * 41)}`}
                                />
                            </div>
                        ))}
                    </div>
                    <div className="grid grid-cols-2 gap-4 mt-4 text-sm text-gray-600">
                        <div className="flex items-center gap-2">
                            <div className="w-4 h-4 bg-red-500 rounded"></div>
                            <span>Temperature</span>
                        </div>
                        <div className="flex items-center gap-2">
                            <div className="w-4 h-4 bg-blue-500 rounded"></div>
                            <span>Gas Level</span>
                        </div>
                    </div>
                </div>

                <div className="bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6">
                    <h2 className="text-2xl font-bold text-gray-800 mb-6">🎛️ Manual Controls</h2>

                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">

                        <button
                            className="bg-gradient-to-r from-red-500 to-red-600 hover:from-red-600 hover:to-red-700 text-white font-bold py-6 px-8 rounded-2xl transition transform hover:scale-105 shadow-lg"
                            onClick={() => {
                                setAlertActive(false);
                                alert('🔕 Alert has been reset!');
                            }}
                        >
                            <div className="text-3xl mb-2">🔕</div>
                            <div className="text-lg">Reset Alert</div>
                            <div className="text-xs opacity-75 mt-1">Stop alarm sound</div>
                        </button>

                        <button
                            className={`${fanManual ? 'bg-gradient-to-r from-blue-700 to-blue-800' : 'bg-gradient-to-r from-blue-500 to-blue-600'} hover:from-blue-600 hover:to-blue-700 text-white font-bold py-6 px-8 rounded-2xl transition transform hover:scale-105 shadow-lg`}
                            onClick={() => setFanManual(!fanManual)}
                        >
                            <div className="text-3xl mb-2">🌀</div>
                            <div className="text-lg">Emergency Fan: {fanManual ? 'ON' : 'OFF'}</div>
                            <div className="text-xs opacity-75 mt-1">Manual override</div>
                        </button>
                    </div>

                    {fanManual && (
                        <div className="mt-4 bg-blue-100 border-l-4 border-blue-500 p-4 rounded">
                            <p className="text-blue-700 font-semibold">
                                ℹ️ Emergency fan is running
                            </p>
                        </div>
                    )}
                </div>

                <div className="mt-6 bg-white/95 backdrop-blur rounded-3xl shadow-xl p-6">
                    <h3 className="text-xl font-bold text-gray-800 mb-4">💡 Status Indicators</h3>
                    <div className="grid grid-cols-3 gap-4">
                        <div className={`p-4 rounded-xl ${data.status === 'normal' ? 'bg-green-500' : 'bg-gray-300'} transition`}>
                            <div className="text-white text-center">
                                <div className="text-2xl mb-2">✅</div>
                                <div className="font-bold">NORMAL</div>
                            </div>
                        </div>
                        <div className={`p-4 rounded-xl ${data.status === 'warning' ? 'bg-yellow-500' : 'bg-gray-300'} transition`}>
                            <div className="text-white text-center">
                                <div className="text-2xl mb-2">⚠️</div>
                                <div className="font-bold">WARNING</div>
                            </div>
                        </div>
                        <div className={`p-4 rounded-xl ${data.status === 'critical' ? 'bg-red-500' : 'bg-gray-300'} transition`}>
                            <div className="text-white text-center">
                                <div className="text-2xl mb-2">🔴</div>
                                <div className="font-bold">CRITICAL</div>
                            </div>
                        </div>
                    </div>
                </div>

                <div className="mt-6 text-center text-white text-sm opacity-75">
                    <p>🖥️ Smart Server Room Guardian Pro - Simulator Mode</p>
                    <p className="text-xs mt-1">Data is simulated for demonstration purposes</p>
                </div>

            </div>
        </div>
    );
}